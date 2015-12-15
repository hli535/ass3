#include  <unistd.h>
#include  <sys/types.h>       /* basic system data types */
#include  <sys/socket.h>      /* basic socket definitions */
#include  <netinet/in.h>      /* sockaddr_in{} and other Internet defns */
#include  <arpa/inet.h>       /* inet(3) functions */
#include <sys/stat.h>
#include <netdb.h> /*gethostbyname function */
#include <getopt.h>

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <math.h>
#include <fcntl.h>

#include "hftp.h"
#include "common.h"

#define MAXLINE 1024

#define CLIENT_STATE_WAIT 0
#define CLIENT_STATE_CONNECTED 1
#define CLIENT_STATE_WAIT_CLOSE 2
#define CLIENT_STATE_CLOSE 3

struct option_param {
  char host[64];
  int port;
  int verbose;
};

//Get the chunk number of file.
int get_chunk_number(const char* filename) {
  struct stat statbuf;
  if (stat(filename, &statbuf) == -1)
    return -1;
  return ceil(1.0 * statbuf.st_size / MSG_DATA_DATA_MAX_LEN);
}

//Get the filesize.
int get_filesize(const char* filename) {
  struct stat statbuf;
  if (stat(filename, &statbuf) == -1)
    return -1;
  return statbuf.st_size;
}

//Get the chunk data of file in chunk_index
int get_chunk(int fd, int chunk_index, unsigned char* data) {
  int len = 0;
  lseek(fd, SEEK_SET, chunk_index * MSG_DATA_DATA_MAX_LEN);
  len = read(fd, data, MSG_DATA_DATA_MAX_LEN);
  return len;
}

//Parse main parameter.
void get_client_param(int argc, char* argv[],char* host, int *port, int *verbose) {
  int opt;
  struct option longopts[] = {
    {"fserver",1,NULL,'f'},
    {"fport",1,NULL,'o'},
    {"verbose",1,NULL,'v'},
    {0,0,0,0}};

  strcpy(host, "localhost");
  *port = 10000;
  *verbose = 0;

  while((opt = getopt_long(argc, argv, ":f:o:v", longopts, NULL)) != -1){
    switch(opt){
      case 'o':
        *port = atoi(optarg);
        break;
      case 'f':
        strcpy(host, optarg);
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

//Handle function.
void handle(int sockfd, struct pollfd* fds, struct option_param * opt);


int main(int argc, char **argv)
{
  char buf[MAXLINE];
  int connfd;

  struct sockaddr_in servaddr;
  struct pollfd fds;
  struct option_param opt = {"127.0.0.1", 6888 ,1};

  get_client_param(argc, argv, (opt.host), &(opt.port), &(opt.verbose));
  printf("host:%s port:%d verbose:%d\n", opt.host, opt.port, opt.verbose);

  connfd = socket(AF_INET, SOCK_DGRAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(opt.port);
  inet_pton(AF_INET, opt.host, &servaddr.sin_addr);

  if (connect(connfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror("connect error");
    return -1;
  }

  fds.fd = connfd;
  fds.events = POLLIN;

  handle(connfd, &fds, &opt);     /* do it all */
  close(connfd);
  printf("exit\n");
  exit(0);
}

void handle(int sockfd, struct pollfd* fds, struct option_param* opt)
{
  char sendline[MAXLINE], recvline[MAXLINE];
  int n, i, j;
  int ret = 0;
  int time_out = 4000;
  char buf[2048];
  int buf_len;
  int current_state = CLIENT_STATE_WAIT;
  int data_len = 2;
  int data_index = 0;
  int return_len;
  int current_seq = 0;
  int fd = 0;
  char mytemp[2048] = {1};
  char token[32];
  char root[256] = "/home/vagrant/cs3357";
  struct message_control msg_ctl_start, msg_ctl_term;
  struct message_data msg_data;
  struct message_response msg_resp;
  int cur_chunk, sum_chunk;
  char filenames[100][256] = {"test1.txt", "test2.txt", "test3.txt", "test4.txt", "apue.3e.pdf"};
  unsigned checksum[100] = {0};
  int cur_filename_index, sum_filename_index;
  struct st_file data[1024], data_std[1024];
  int st_file_len = 0, data_std_len = 0;
  char files[4096] = {0};

  cur_filename_index = 0;
  sum_filename_index = 5;
  cur_chunk = 0;

  memset(data, sizeof(data[0])*100, 0);
  get_file_list("127.0.0.1", 9000, "jeff", "password", root, files, data_std,
      &data_std_len);
  get_token("127.0.0.1", 9000, "jeff", "password", token);
  parse_files(files, data, &st_file_len);
  for (i=0; i!=st_file_len; ++i) {
    for (j=0; j!=data_std_len; ++j) {
      if (strcmp(data[i].filename, data_std[j].filename) == 0) {
        data[i].checksum = data_std[j].checksum;
      }
    }
  }
  
  for (i=0; i!=st_file_len; ++i) {
    snprintf(filenames[i], sizeof(filenames[0]), "%s", data[i].filename);
    checksum[i] = (data[i].checksum);
  }
  sum_filename_index = st_file_len;

  for (;;) {
    if (CLIENT_STATE_WAIT == current_state) { //State wait to connect server.
      msg_ctl_start.type = 1;
      msg_ctl_start.seq = current_seq;
      msg_ctl_start.filename_len = strlen(filenames[cur_filename_index]);
      msg_ctl_start.filesize = get_filesize(filenames[cur_filename_index]);
      msg_ctl_start.checksum = checksum[cur_filename_index];
      memcpy(msg_ctl_start.token, token, 16);
      memcpy(msg_ctl_start.filename, filenames[cur_filename_index], msg_ctl_start.filename_len);

      snprintf(mytemp, sizeof(mytemp)-1, "%s/%s", root, filenames[cur_filename_index]);
      
      sum_chunk = get_chunk_number(mytemp);
      fd = open(mytemp, O_RDONLY);
      printf("filename:%s fileindex:%d filesize:%d\n",
          mytemp,
          cur_filename_index + 1,
          get_filesize(mytemp));

      message_control_to_bytes(&msg_ctl_start, buf, &buf_len);
      if (opt->verbose == 1)
        print_msg_ctl(&msg_ctl_start);
    } else if (CLIENT_STATE_CONNECTED == current_state) { //Connected.
      if (cur_filename_index == sum_filename_index - 1 &&
          cur_chunk == sum_chunk - 1) { // all filed sended.
        printf("filename:[%s] sum_chunk:[%d] chunk:[%d]\n", filenames[cur_filename_index], sum_chunk, cur_chunk);
        msg_data.type = 3;
        msg_data.seq = current_seq;
        msg_data.data_len = get_chunk(fd, cur_chunk, msg_data.data);
        message_data_to_bytes(&msg_data, buf, &buf_len);

        if (opt->verbose == 1)
          print_msg_data(&msg_data);
        current_state = CLIENT_STATE_WAIT_CLOSE;
        close(fd);
      } else {
        if (cur_chunk == sum_chunk) {    //one file sended and init next file.
          ++cur_filename_index;
          cur_chunk = 0;
          close(fd);

      snprintf(mytemp, sizeof(mytemp)-1, "%s/%s", root, filenames[cur_filename_index]);

          msg_ctl_start.type = 1;
          msg_ctl_start.seq = current_seq;
          msg_ctl_start.filename_len = strlen(filenames[cur_filename_index]);
          msg_ctl_start.filesize = get_filesize(mytemp);
          msg_ctl_start.checksum = checksum[cur_filename_index];
          memcpy(msg_ctl_start.token, token, 16);
          memcpy(msg_ctl_start.filename, filenames[cur_filename_index], msg_ctl_start.filename_len);
          sum_chunk = get_chunk_number(mytemp);
          fd = open(mytemp, O_RDONLY);
          printf("filename:%s fileindex:%d filesize:%d\n",
              mytemp,
              cur_filename_index + 1,
              get_filesize(mytemp));

          message_control_to_bytes(&msg_ctl_start, buf, &buf_len);
          if (opt->verbose == 1)
            print_msg_ctl(&msg_ctl_start);
        } else { //Send data to server.
          printf("filename:[%s] sum_chunk:[%d] chunk:[%d]\n", filenames[cur_filename_index], sum_chunk, cur_chunk);
          msg_data.type = 3;
          msg_data.seq = current_seq;
          msg_data.data_len = get_chunk(fd, cur_chunk, msg_data.data);
          message_data_to_bytes(&msg_data, buf, &buf_len);

          ++cur_chunk;
          if (opt->verbose == 1)
            print_msg_data(&msg_data);
        }
      }
    } else if (CLIENT_STATE_CLOSE == current_state) { //To close.
      snprintf(mytemp, sizeof(mytemp)-1, "%s/%s", root, filenames[cur_filename_index]);

      msg_ctl_term.type = 2;
      msg_ctl_term.seq = current_seq;
      msg_ctl_term.filename_len = strlen(filenames[cur_filename_index]);
      msg_ctl_term.filesize = get_filesize(mytemp);
      msg_ctl_term.checksum = checksum[cur_filename_index];
      memcpy(msg_ctl_term.token, token, 16);
      memcpy(msg_ctl_term.filename, filenames[cur_filename_index], msg_ctl_term.filename_len);
      message_control_to_bytes(&msg_ctl_term, buf, &buf_len);
      if (opt->verbose == 1)
        print_msg_ctl(&msg_ctl_term);
    }
    n = write(sockfd, buf, buf_len);
    //sendto(sockfd, buf, strlen(buf),0,(struct sockaddr*)&caddr, sizeof(caddr));  
    ret = poll(fds, 1, time_out); 
    if (-1 == ret) {
      perror("poll()");  
    } else if (0 == ret) {
      printf("time out\n");
    } else if (ret > 0) {
      if( ( fds[0].revents & POLLIN ) ==  POLLIN ){
        struct sockaddr_in addr;
        char ipbuf[INET_ADDRSTRLEN] = "";
        socklen_t addrlen = sizeof(addr);
        bzero(&addr,sizeof(addr));

        return_len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        if (CLIENT_STATE_WAIT == current_state) {
          bytes_to_message_response(&msg_resp, buf, sizeof(buf));
          if (opt->verbose == 1)
            print_msg_resp(&msg_resp);
          if (msg_resp.type == 255 && msg_resp.error_code != 1 && msg_resp.seq == !current_seq) {
            current_state = CLIENT_STATE_CONNECTED;
            current_seq = !current_seq;
          } else {
            printf("error\n");
          }
        } else if (CLIENT_STATE_CONNECTED == current_state){
          bytes_to_message_response(&msg_resp, buf, sizeof(buf));
          if (opt->verbose == 1)
            print_msg_resp(&msg_resp);
          if (msg_resp.type == 255 && msg_resp.error_code != 1 && msg_resp.seq == !current_seq) {
            current_state = CLIENT_STATE_CONNECTED;
            current_seq = !current_seq;
          } else {
            printf("error\n");
          }
        } else if (CLIENT_STATE_WAIT_CLOSE == current_state) {
          bytes_to_message_response(&msg_resp, buf, sizeof(buf));
          if (opt->verbose == 1)
            print_msg_resp(&msg_resp);
          if (msg_resp.type == 255 && msg_resp.error_code != 1 && msg_resp.seq == !current_seq) {
            current_state = CLIENT_STATE_CLOSE;
            current_seq = !current_seq;
            //break;
          } else {
            printf("error\n");
          }
        } else if (CLIENT_STATE_CLOSE == current_state) {
          bytes_to_message_response(&msg_resp, buf, sizeof(buf));
          if (opt->verbose == 1)
            print_msg_resp(&msg_resp);
          if (msg_resp.type == 255 && msg_resp.error_code != 1 && msg_resp.seq == !current_seq) {
            break;
          } else {
            printf("error\n");
          }
        }
      }
    }
    if (n == 0) {
      printf("echoclient: server terminated prematurely\n");
      break;
    }
  }
}
