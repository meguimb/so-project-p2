#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

// char *concatenate_args(int opCode, char *name, int session_id, int  flags, int fhandle, size_t len);
void *send_args(char opCode, void const *name, int session_id, int  flags, int fhandle, size_t len);

int active_session_id;
int pipe_client;
int pipe_server;
char* _client_pipe_path;
char const*_server_pipe_path;
int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    _server_pipe_path = server_pipe_path;
    _client_pipe_path = client_pipe_path;
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return 0;
    }
    if (mkfifo(client_pipe_path, 0640) < 0) {
        return -1;
    }
    pipe_server = open(server_pipe_path, O_WRONLY);
    if (pipe_server == -1) {
        return -1;
    }
    void *send_req_str = send_args((char) TFS_OP_CODE_MOUNT, client_pipe_path, -1, -1, -1, 0);
    size_t size = ((size_t *) send_req_str)[0];
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    free(send_req_str);

    pipe_client = open(client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    
    // receber int de session_id do server
    if (read(pipe_client, &active_session_id, sizeof(int)) < 0) {
        return -1;
    }
    close(pipe_client);
    return 0;
}

int tfs_unmount() {
    size_t str_len;
    int ret_val;

    void *send_req_str = send_args( (char)TFS_OP_CODE_UNMOUNT, NULL, active_session_id, -1, -1, 0);
    str_len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, str_len) < 0) {
        return -1;
    }
    // receber int de retorno do server
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    if (ret_val == -1){
        printf("error with tfs_close ocurred\n");
        return -1;
    }
    // fechar pipes
    if (close(pipe_client) < 0) {
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
    size_t size;
    int ret_val;
    void *send_req_str = send_args( (char) TFS_OP_CODE_OPEN, name, active_session_id, flags, -1, 0);

    size = ((size_t *) send_req_str)[0];
    char *str = (char *) send_req_str;
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    printf("written to server sucessfully\n");
    free(send_req_str);

    // open client pipe to read
    printf("open client pipe to read\n");
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    printf("opened sucessfully\n");
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    if (ret_val == -1){
        printf("error with tfs_close ocurred\n");
        return -1;
    }
    close(pipe_client);
    return 0;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    size_t str_len;
    int ret_val;
    void *send_req_str = send_args( (char) TFS_OP_CODE_CLOSE, NULL, active_session_id, -1, fhandle, 0);
    str_len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, str_len) < 0) {
        return -1;
    }
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    if (ret_val == -1){
        printf("error with tfs_close ocurred\n");
        return -1;
    }
    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    size_t str_len;
    int session_id = active_session_id;
    void *send_req_str = send_args( (char) TFS_OP_CODE_WRITE, buffer, session_id, -1, fhandle, len);
    str_len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, str_len) < 0) {
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
    size_t str_len, nOfBytesRead;
    int session_id = active_session_id;
    void *send_req_str = send_args((char) TFS_OP_CODE_READ, NULL, session_id, -1, fhandle, len);
    // nao se manda o buffer para o server
    str_len = strlen(send_req_str);
    if (write(pipe_server, send_req_str, str_len) < 0) {
        return -1;
    }
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &nOfBytesRead, sizeof(size_t)) < 0) {
        return -1;
    }
    if (nOfBytesRead == -1){
        printf("error ocurred with tfs_read\n");
    }
    else if (nOfBytesRead == strlen(buffer)){
        printf("everything went smoothyly\n");
    }
    return 0;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}

void *send_args(char opCode, void const *name, int session_id, int  flags, int fhandle, size_t len){
    size_t str_len, pipe_buf_size;
    str_len = strlen(name)+1;
    pipe_buf_size = (size_t) (sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + 40 + sizeof(int)); 
    void *request_msg = malloc(pipe_buf_size);
    void *ptr = request_msg;
    // put number of bytes allocated in the beginning of the pointer
    memcpy(request_msg, &pipe_buf_size, sizeof(size_t));
    request_msg += sizeof(size_t);
    // concatenate opCode
    memcpy(request_msg, &opCode, sizeof(char));
    request_msg += sizeof(char);

    // concatenate session_id if necessary
    if (session_id != -1){
        memcpy(request_msg, &session_id, sizeof(int));
        request_msg += sizeof(int);
    }
    
    // concatenate fhandle if necessary
    if (fhandle != -1){
        memcpy(request_msg, &fhandle, sizeof(int));
        request_msg += sizeof(int);
    }
    if (len != 0){
        memcpy(request_msg, &len, sizeof(size_t));
        request_msg += sizeof(size_t);
    }
    if (name != NULL){
        memcpy(request_msg, name, str_len);
        request_msg += 40;
    }
    if (flags != -1){
        memcpy(request_msg, &flags, sizeof(int));
        request_msg += sizeof(int);
    }
    return ptr;
}
