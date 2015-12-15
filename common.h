#ifndef COMMON_H_
#define COMMON_H_

#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
#include <hmdp.h>
#include <hmdp_request.h>
#include <hmdp_response.h>

#include <hfs.h>

#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

struct st_file {
  char filename[256];
  int checksum;
};

void get_file_list(char* hmds_host, int hmds_port,
    char* username, char* password, char* root, char* files,
    struct st_file* st_data, int* st_data_len);

void get_token(char* hmds_host, int hmds_port,
    char* username, char* password, char* token);

void parse_files(char* files, struct st_file *data, int* sum_file);

void get_file_list_local(struct st_file* data, int *data_len,
    char* root);

void get_files_crc32(char* root, struct st_file *file, int* len);

#endif //COMMON_H_
