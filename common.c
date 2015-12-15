#include "common.h"

#include <stdlib.h>
#include <string.h>

void get_file_list(char* hmds_host, int hmds_port,
    char* username, char* password, char* root, char* files,
    struct st_file* st_data, int* st_data_len) {
  char *token = NULL;
  struct hmdp_request *req = NULL;
  struct hmdp_response *resp = NULL;
  struct sockaddr_in client_addr;
  char file[512] = {0};
  char files_temp[4096] = {0};
  int i = 0;

  hfs_entry* head = hfs_get_files(root);
  hfs_entry* cur = head;

  while (cur != NULL) {
    snprintf(file, sizeof(file), "%s\n%x", head->rel_path, head->crc32);
    st_data[i].checksum = head->crc32;
    strcpy(st_data[i].filename, head->rel_path);
    if (i != 0)
      strcat(files_temp, "\n");
    strcat(files_temp, file);
    cur = cur->next;
    ++i;
  }
  *st_data_len = i;

  bzero(&client_addr,sizeof(client_addr));
  client_addr.sin_family = AF_INET;   
  client_addr.sin_addr.s_addr = htons(INADDR_ANY);
  client_addr.sin_port = htons(0); 
  int client_socket = socket(AF_INET,SOCK_STREAM,0);
  if( client_socket < 0)
  {
    printf("Create Socket Failed!\n");
    exit(1);
  }
  if( bind(client_socket,(struct sockaddr*)&client_addr,sizeof(client_addr)))
  {
    printf("Client Bind Port Failed!\n"); 
    exit(1);
  }

  struct sockaddr_in server_addr;
  bzero(&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  if(inet_aton(hmds_host ,&server_addr.sin_addr) == 0)
  {
    printf("Server IP Address Error!\n");
    exit(1);
  }
  server_addr.sin_port = htons(hmds_port);
  socklen_t server_addr_length = sizeof(server_addr);
  if(connect(client_socket,(struct sockaddr*)&server_addr, server_addr_length) < 0)
  {
    printf("Can Not Connect To %s!\n",hmds_host);
    exit(1);
  }
  req = hmdp_create_auth_request(username, password);
  hmdp_send_request(req, client_socket);
  resp = hmdp_read_response(client_socket);
  token = hmdp_header_get(resp->headers, "Token");
  req = hmdp_create_list_request(token, files_temp);
  hmdp_send_request(req, client_socket);
  resp = hmdp_read_response(client_socket);
  strcpy(files, resp->body);
  close(client_socket);
}

void get_file_list_local(struct st_file* data, int *data_len,
    char* root) {
  int i = 0;
  hfs_entry* head = hfs_get_files(root);
  hfs_entry* cur = head;

  while (cur != NULL) {
    snprintf(data[i].filename, sizeof(data[0].filename), "%s", head->rel_path);
    data[i].checksum = head->crc32;
    cur = cur->next;
    ++i;
  }
  *data_len = i;
}

void get_token(char* hmds_host, int hmds_port,
    char* username, char* password, char* token) {
  struct hmdp_request *req = NULL;
  struct hmdp_response *resp = NULL;
  struct sockaddr_in client_addr;
  char* tmp = NULL;

  bzero(&client_addr,sizeof(client_addr)); 
  client_addr.sin_family = AF_INET;   
  client_addr.sin_addr.s_addr = htons(INADDR_ANY);
  client_addr.sin_port = htons(0); 
  int client_socket = socket(AF_INET,SOCK_STREAM,0);
  if( client_socket < 0)
  {
    printf("Create Socket Failed!\n");
    exit(1);
  }
  if( bind(client_socket,(struct sockaddr*)&client_addr,sizeof(client_addr)))
  {
    printf("Client Bind Port Failed!\n"); 
    exit(1);
  }

  struct sockaddr_in server_addr;
  bzero(&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  if(inet_aton(hmds_host ,&server_addr.sin_addr) == 0)
  {
    printf("Server IP Address Error!\n");
    exit(1);
  }
  server_addr.sin_port = htons(hmds_port);
  socklen_t server_addr_length = sizeof(server_addr);
  if(connect(client_socket,(struct sockaddr*)&server_addr, server_addr_length) < 0)
  {
    printf("Can Not Connect To %s!\n",hmds_host);
    exit(1);
  }
  req = hmdp_create_auth_request(username, password);
  hmdp_send_request(req, client_socket);
  resp = hmdp_read_response(client_socket);
  tmp = hmdp_header_get(resp->headers, "Token");
  memcpy(token, tmp, 16);
  token[16] = 0;
  close(client_socket);
}

void parse_files(char* files, struct st_file *data, int* sum_file) {
  char *tmp;
  int i = 0;
  if (strlen(files) == 0) {
    *sum_file = 0;
    return;
  }

  tmp = strtok(files, "\n");
  while (tmp != NULL) {
      strcpy(data[i].filename, tmp);
    tmp = strtok(NULL, "\n");
    ++i;
  }
  *sum_file = i;
}

