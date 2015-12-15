#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>

#include <hmdp.h>
#include <hmdp_request.h>
#include <hmdp_response.h>
#include <hdb.h>

#include "hftp.h"

#define SERVER_STATE_WAIT 0
#define SERVER_STATE_CONNECTED 1
#define SERVER_STATE_WAIT_CLOSE 2
#define SERVER_STATE_CLOSE 3

//Main function parameter.
struct option_param {
  int port;
  char redis[64];
  char dir[256];
  int timewait;
  int verbose;
};

//Parse main parameter using getopt_long.
void get_server_param(int argc, char* argv[],int *port, char* redis, char* dir, int* timewait, int* verbose) {
  int opt;
  struct option longopts[] = {
    {"port",1,NULL,'p'},
    {"redis",1,NULL,'r'},
    {"dir",1,NULL,'d'},
    {"timewait",1,NULL,'t'},
    {"verbose",1,NULL,'v'},
    {0,0,0,0}};

  strcpy(redis, "localhost");
  strcpy(dir, "/tmp/hftpd");
  *timewait = 10;
  *verbose = 0;
  *port = 10000;

  while((opt = getopt_long(argc, argv, ":p:r:d:t:v", longopts, NULL)) != -1){
    switch(opt){
      case 'p':
        *port = atoi(optarg);
        break;
      case 'r':
        strcpy(redis, optarg);
        break;
      case 'd':
        strcpy(dir, optarg);
        break;
      case 't':
        *timewait = atoi(optarg);
        break;
      case 'v':
        *verbose = 1;
        break;
      case ':':
        printf("option needs a value\n");
        break;
      case '?':
        printf("unknown option: %c\n",optopt);
        break;
    }
  }
}

//Save metadata to redis server.
void save_to_redis(char* host,
    char* username,
    char* password,
    char* filename,
    char* checksum) {
  char *result = NULL;
  char *token = NULL;
  hdb_connection* con = hdb_connect(host);
  token = hdb_authenticate(con, username, password);
  result = hdb_verify_token(con, token);
  if (result != NULL) {
    hdb_record hdb_rec;
    hdb_rec.filename = filename;
    hdb_rec.username = username;
    hdb_rec.checksum = checksum;
    hdb_rec.next = NULL;
    hdb_store_file(con, &hdb_rec);
  }
}

//Create directory recursively if directory does not exist.
void mkdirr(char *dir) {
  char tmp[256];
  char *p = NULL;
  int i = 0;
  size_t len;

  snprintf(tmp, sizeof(tmp),"%s",dir);
  for(i = len-1; i>0; --i)
    if(tmp[i] == '/') {
      tmp[i] = 0;
      break;
    }
  len = strlen(tmp);
  if(tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  for(p = tmp + 1; *p; p++)
    if(*p == '/') {
      *p = 0;
      mkdir(tmp, S_IRWXU);
      *p = '/';
    }
  mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);
}

int main(int argc,char *argv[])
{
  int udpfd = 0, ret = 0, time_out = -1;
  int current_state = SERVER_STATE_WAIT;
  int buf_len = 0, current_seq = 0, return_code = 0;
  struct pollfd fds;
  struct sockaddr_in saddr;
  struct message_control msg_ctl;
  struct message_data msg_data;
  struct message_response msg_resp;

  char mytemp[2048];
  int mytemp_len;
  int fd;
  char filename[256] = {0};
  char filename_temp[256] = {0};
  struct option_param opt;
  int is_dir = 0;

  memset(&opt, sizeof(opt), 0);
  opt.port = 9000;
  get_server_param(argc, argv, &(opt.port), opt.redis, opt.dir, &(opt.timewait), &(opt.verbose));
  if (strlen(opt.dir) == 0)
    is_dir = 0;
  else
    is_dir = 1;
  printf("port:%d redis:%s dir:%s timewait:%d verbose:%d\n",
      opt.port, opt.redis, opt.dir, opt.timewait, opt.verbose);

  msg_resp.type = 255;
  msg_resp.error_code = 0;

  bzero(&saddr,sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port   = htons(opt.port);
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if( (udpfd = socket(AF_INET,SOCK_DGRAM, 0)) < 0)
  {
    perror("socket error");
    exit(-1);
  }

  if(bind(udpfd, (struct sockaddr*)&saddr, sizeof(saddr)) != 0)
  {
    perror("bind error");
    close(udpfd);		
    exit(-1);
  }

  fds.fd = udpfd;	//udp描述符

  fds.events = POLLIN; // 普通或优先级带数据可读

  while(1)
  {	
    ret = poll(&fds, 1, time_out); 

    if(ret == -1){
      perror("poll()");  
    }
    else if(ret > 0){
      char buf[2048] = {0};  
      if( ( fds.revents & POLLIN ) ==  POLLIN ){
        struct sockaddr_in addr;
        char ipbuf[INET_ADDRSTRLEN] = "";
        socklen_t addrlen = sizeof(addr);

        bzero(&addr,sizeof(addr));

        recvfrom(udpfd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        printf("hello\n");
        if (SERVER_STATE_WAIT == current_state) {       //Wait to be connected.
          bytes_to_message_control(&msg_ctl, buf, sizeof(buf));
          if (msg_ctl.type == 1 && msg_ctl.seq == current_seq) {
            current_seq = !current_seq;
            msg_resp.seq = current_seq;
            message_response_to_bytes(&msg_resp, buf, &buf_len);
            return_code = sendto(udpfd, buf, buf_len, 0, (struct sockaddr*)&addr, sizeof(addr));
            current_state = SERVER_STATE_CONNECTED;
            if (opt.verbose) {
              print_msg_ctl(&msg_ctl);
              print_msg_resp(&msg_resp);
            }

            memcpy(filename, msg_ctl.filename, msg_ctl.filename_len);
            filename[msg_ctl.filename_len + 1] = 0;
            printf("%s\n", filename);

            snprintf(filename_temp, sizeof(filename_temp) - 1, "tmp/%s", filename);
            if (is_dir == 0) {
              snprintf(filename_temp, sizeof(filename_temp) - 1, "/tmp/hftpd/%s", filename);
            } else {
              snprintf(filename_temp, sizeof(filename_temp) - 1, "%s/%s", opt.dir, filename);
            }
            snprintf(mytemp, sizeof(mytemp), "%x", msg_ctl.checksum);
            //save_to_redis(opt.redis, "jeff", "password", filename, mytemp);

            mkdirr(filename_temp);
            if(-1 != (fd = open(filename_temp, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO))) {
              printf("open failed %s\n", filename_temp);
            }
          } else {
            printf("error\n");
          }
        } else if (SERVER_STATE_CONNECTED == current_state) {
          bytes_to_message_data(&msg_data, buf, sizeof(buf));
          if (msg_data.type == 3 && msg_data.seq == current_seq) {
            current_seq = !current_seq;
            msg_resp.seq = current_seq;
            message_response_to_bytes(&msg_resp, buf, &buf_len);
            sendto(udpfd, buf, buf_len, 0, (struct sockaddr*)&addr, sizeof(addr));
            if (opt.verbose) {
              print_msg_data(&msg_data);
              print_msg_resp(&msg_resp);
            }
            write(fd, msg_data.data, msg_data.data_len);
          } else {
            bytes_to_message_control(&msg_ctl, buf, sizeof(buf));
            printf("type:%d\n", msg_ctl.type);
            if (msg_ctl.type == 2 && msg_ctl.seq == current_seq) {
              current_seq = !current_seq;
              msg_resp.seq = current_seq;
              message_response_to_bytes(&msg_resp, buf, &buf_len);
              sendto(udpfd, buf, buf_len, 0, (struct sockaddr*)&addr, sizeof(addr));
              if (opt.verbose) {
                print_msg_ctl(&msg_ctl);
                print_msg_resp(&msg_resp);
              }
              current_state = SERVER_STATE_WAIT;
              current_seq = 0;
            } else if (msg_ctl.type == 1 && msg_ctl.seq == current_seq) {
              current_seq = !current_seq;
              msg_resp.seq = current_seq;
              message_response_to_bytes(&msg_resp, buf, &buf_len);
              sendto(udpfd, buf, buf_len, 0, (struct sockaddr*)&addr, sizeof(addr));
              if (opt.verbose) {
                print_msg_ctl(&msg_ctl);
                print_msg_resp(&msg_resp);
              }

              close(fd);
              memcpy(filename, msg_ctl.filename, msg_ctl.filename_len);
              filename[msg_ctl.filename_len + 1] = 0;
              printf("%s\n", filename);
              snprintf(filename_temp, sizeof(filename_temp) - 1, "tmp/%s", filename);
              if (is_dir == 0) {
                snprintf(filename_temp, sizeof(filename_temp) - 1, "/tmp/hftpd/%s", filename);
              } else {
                snprintf(filename_temp, sizeof(filename_temp) - 1, "%s/%s", opt.dir, filename);
              }
              snprintf(mytemp, sizeof(mytemp), "%x", msg_ctl.checksum);
              save_to_redis(opt.redis, "jeff", "password", filename, mytemp);
              mkdirr(filename_temp);
              if(-1 != (fd = open(filename_temp, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO))) {
                printf("open failed %s\n", filename_temp);
              }
            } else {
              printf("error\n");
            }
          }
        }
      }
    }
    else if(0 == ret){          //timeout.
      printf("time out\n");  
    }  
  }
  return 0;
}

