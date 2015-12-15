#include "hftp.h"

void message_control_to_bytes(struct message_control* msg,
    unsigned char* dest, int* dest_len) {
  memcpy(dest, (unsigned char*)(&msg->type), 1);
  memcpy(dest+1, (unsigned char*)(&msg->seq), 1);
  memcpy(dest+2, (unsigned char*)(&msg->filename_len), 2);
  memcpy(dest+4, (unsigned char*)(&msg->filesize), 4);
  memcpy(dest+8, (unsigned char*)(&msg->checksum), 4);
  memcpy(dest+12, (unsigned char*)(msg->token), 16);
  memcpy(dest+28, (unsigned char*)(msg->filename),
      msg->filename_len); //max length is 1444 types.
  *dest_len = 28 + msg->filename_len;
}

void bytes_to_message_control(struct message_control* msg,
    unsigned char* dest, int dest_len) {
  memcpy((unsigned char*)(&msg->type), dest, 1);
  memcpy((unsigned char*)(&msg->seq), dest+1, 1);
  memcpy((unsigned char*)(&msg->filename_len), dest+2, 2);
  memcpy((unsigned char*)(&msg->filesize), dest+4, 4);
  memcpy((unsigned char*)(&msg->checksum), dest+8, 4);
  memcpy((unsigned char*)(msg->token), dest+12, 16);
  memcpy((unsigned char*)(msg->filename), dest+28, msg->filename_len);
}


void message_data_to_bytes(struct message_data* msg,
    unsigned char* dest, int* dest_len) {
  memcpy(dest, (unsigned char*)(&msg->type), 1);
  memcpy(dest+1, (unsigned char*)(&msg->seq), 1);
  memcpy(dest+2, (unsigned char*)(&msg->data_len), 2);
  memcpy(dest+4, (unsigned char*)(msg->data),
      msg->data_len); //max length is 1468 bytes.
  *dest_len = 4 + msg->data_len;
}

void bytes_to_message_data(struct message_data* msg,
    unsigned char* dest, int dest_len) {
  memcpy((unsigned char*)(&msg->type), dest, 1);
  memcpy((unsigned char*)(&msg->seq), dest+1, 1);
  memcpy((unsigned char*)(&msg->data_len), dest+2, 2);
  memcpy((unsigned char*)(msg->data), dest+4,
      msg->data_len); //max length is 1468 bytes.
}

void message_response_to_bytes(struct message_response* msg,
    unsigned char* dest, int* dest_len) {
  memcpy(dest, (unsigned char*)(&msg->type), 1);
  memcpy(dest+1, (unsigned char*)(&msg->seq), 1);
  memcpy(dest+2, (unsigned char*)(&msg->error_code), 2);
  *dest_len = 4;
}

void bytes_to_message_response(struct message_response* msg,
    unsigned char* dest, int dest_len) {
  memcpy((unsigned char*)(&msg->type), dest, 1);
  memcpy((unsigned char*)(&msg->seq), dest+1, 1);
  memcpy((unsigned char*)(&msg->error_code), dest+2, 2);
}

void print_msg_ctl(struct message_control* msg) {
  printf("\nmessage_control\n");
  if (msg->type == 1) {
    printf("type:         control message initialise\n");
  } else if (2 == msg->type){
    printf("type:         control message terminated\n");
  }
  printf("seq:          %d\n", msg->seq);
  printf("filename_len: %d\n", msg->filename_len);
  printf("filesize:     %d\n", msg->filesize);
  printf("checksum:     %x\n", msg->checksum);
}

void print_msg_data(struct message_data* msg) {
  printf("\nmessage_data\n");
  printf("type:         %d\n", msg->type);
  printf("seq:          %d\n", msg->seq);
  printf("data_len:     %d\n", msg->data_len);
}

void print_msg_resp(struct message_response *msg) {
  printf("\nmessage_response\n");
  printf("type:         %d\n", msg->type);
  printf("seq:          %d\n", msg->seq);
  printf("error_code:   %d\n", msg->error_code);
}

