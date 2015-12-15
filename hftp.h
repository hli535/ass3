#ifndef HFTP_H_
#define HFTP_H_

#include <stdio.h>
#include <string.h>

#define MSG_CONTROL_DATA_MAX_LEN 1444
#define MSG_DATA_DATA_MAX_LEN    1468

#define MSG_CTL_INIT 1
#define MSG_CTL_TERM 2

struct message_control {
  unsigned char type;
  unsigned char seq;
  unsigned int filename_len;
  unsigned int filesize;
  unsigned int checksum;
  unsigned char token[16];
  unsigned char filename[MSG_CONTROL_DATA_MAX_LEN];
};

struct message_data {
  unsigned char type;
  unsigned char seq;
  unsigned int data_len;
  unsigned char data[MSG_DATA_DATA_MAX_LEN];
};

struct message_response {
  unsigned char type;
  unsigned char seq;
  unsigned int error_code;
};

void message_control_to_bytes(struct message_control* msg,
    unsigned char* dest, int* dest_len);

void bytes_to_message_control(struct message_control* msg,
    unsigned char* dest, int dest_len);

void message_data_to_bytes(struct message_data* msg,
    unsigned char* dest, int* dest_len);

void bytes_to_message_data(struct message_data* msg,
    unsigned char* dest, int dest_len);

void message_response_to_bytes(struct message_response* msg,
    unsigned char* dest, int* dest_len);

void bytes_to_message_response(struct message_response* msg,
    unsigned char* dest, int dest_len);

void print_msg_ctl(struct message_control* msg);

void print_msg_data(struct message_data* msg);

void print_msg_resp(struct message_response *msg);

#endif //HFTP_H_
