#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *concatenate_args(int opCode, char *name, int session_id, int  flags, int fhandle, size_t len);
void *send_args(int opCode, char *name, int session_id, int  flags, int fhandle, size_t len);

int active_session_id;
int pipe_client;
int pipe_server;
char* _client_pipe_path;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    if (unlink(client_pipe_path)!=0) {
        return -1;
    }
    if (mkfifo(client_pipe_path,0640)!=0) {
        return -1;
    }
    pipe_server = open(server_pipe_path, O_WRONLY);
    if (pipe_server == -1) {
        return -1;
    }
    strcpy(_client_pipe_path,client_pipe_path);
    char* msg = TFS_OP_CODE_MOUNT;
    strcat(msg," | ");
    strcat(msg,client_pipe_path);
    size_t len = strlen(msg);
    if (write(pipe_server,msg,len) < 0) {
        return -1;
    }
    pipe_client = open(client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client,&active_session_id,1) < 0) {
        return -1;
    }
    return 0;
}

int tfs_unmount() {
    char* msg = TFS_OP_CODE_UNMOUNT;
    strcat(msg, " | ");
    strcat(msg,active_session_id);
    int len = 5; // mudar depois
    if (write(pipe_server,msg,len) < 0) {
        return -1;
    }
    if (close(pipe) < 0) {
        return -1;
    }
    if (close(pipe_server) < 0) {
        return -1;
    }
    if (unlink(_client_pipe_path)!=0) {
        return -1;
    }
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    int opCode = TFS_OP_CODE_WRITE, session_id = active_session_id;
    char *send_req_str = concatenate_args(opCode, buffer, session_id, -1, fhandle, len);
    len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, len) < 0) {
        return -1;
    }
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client,&active_session_id,1) < 0) {
        return -1;
    }
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    int len, opCode = TFS_OP_CODE_READ, session_id = active_session_id;
    char *send_req_str = concatenate_args(opCode, NULL, session_id, -1, fhandle, len);
    len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, len) < 0) {
        return -1;
    }
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client,&active_session_id,1) < 0) {
        return -1;
    }
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}

char *concatenate_args(int opCode, char *name, int session_id, int  flags, int fhandle, size_t len){
    char str [40], int_to_char_buffer [10];
    char *send_msg_buffer;
    int nOfDigitsSessionId = session_id >= 0? 6 : 0;
    int nOfDigitsFhandle = fhandle >= 0? 6 : 0;
    int nOfDigitsLen = len >= 0? 8 : 0;
    strcpy(str, name);
    // bytes of: char(opCode) + char(session_id) + char(fhandle) + char (len) + char[len] + char(flags) + max num of |'s
    int pipe_buf_size = sizeof(char) + nOfDigitsSessionId + nOfDigitsFhandle + nOfDigitsLen + strlen(name)+1 + sizeof(int) + 4; 
    send_msg_buffer = (char *) malloc(pipe_buf_size);
    sprintf(int_to_char_buffer, "%d", opCode);
    strcat(send_msg_buffer, int_to_char_buffer);
    strcat(send_msg_buffer, "|");
    sprintf(int_to_char_buffer, "%d", session_id);
    strcat(send_msg_buffer, int_to_char_buffer);
    if (fhandle != -1){
        strcat(send_msg_buffer, "|");
        sprintf(int_to_char_buffer, "%d", fhandle);
        strcat(send_msg_buffer, int_to_char_buffer);
    }
    if (len != -1){
        strcat(send_msg_buffer, "|");
        sprintf(int_to_char_buffer, "%d", len);
        strcat(send_msg_buffer, int_to_char_buffer);
    }
    if (name != NULL){
        strcat(send_msg_buffer, "|");
        strcat(send_msg_buffer, name);
    }
    if (flags != -1){
        strcat(send_msg_buffer, "|");
        sprintf(int_to_char_buffer, "%d", flags);
        strcat(send_msg_buffer, int_to_char_buffer);
    }
    

    return send_msg_buffer;
}

void *send_args(int opCode, char *name, int session_id, int  flags, int fhandle, size_t len){
    int str_len, pipe_buf_size;
    str_len = strlen(name)+1;
    pipe_buf_size = sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + str_len + sizeof(int); 
    void *request_msg = malloc(pipe_buf_size);

    // concatenate opCode
    memcpy(request_msg, opCode, sizeof(int));
    request_msg += sizeof(int);

    // concatenate session_id
    memcpy(request_msg, session_id, sizeof(int));
    request_msg += sizeof(int);
    
    // concatenate fhandle if necessary
    if (fhandle != -1){
        memcpy(request_msg, fhandle, sizeof(int));
        request_msg += sizeof(int);
    }
    if (len != -1){
        memcpy(request_msg, len, sizeof(size_t));
        request_msg += sizeof(size_t);
    }
    if (name != NULL){
        memcpy(request_msg, name, str_len);
        request_msg += sizeof(str_len);
    }
    if (flags != -1){
        memcpy(request_msg, flags, sizeof(int));
        request_msg += sizeof(int);
    }
    return request_msg;
}
