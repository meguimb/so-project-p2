#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

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
        close(pipe_client);
        return -1;
    }
    printf("session id is %d\n", active_session_id);
    close(pipe_client);
    return 0;
}

int tfs_unmount() {
    size_t str_len;
    int ret_val;

    void *send_req_str = send_args( (char)TFS_OP_CODE_UNMOUNT, NULL, active_session_id, -1, -1, 0);
    size_t size = ((size_t *) send_req_str)[0];
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    printf("request written\n");
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    // receber int de session_id do server
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        printf("read gone wrong\n");
        close(pipe_client);
        return -1;
    }
    printf("ret_val is %d\n", ret_val);
    if (ret_val == -1){
        printf("error with tfs_unmount ocurred\n");
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
    return 0;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    size_t size;
    int ret_val;
    printf("before send_args\n");
    void *send_req_str = send_args( (char) TFS_OP_CODE_OPEN, name, active_session_id, flags, -1, 0);

    size = ((size_t *) send_req_str)[0];
    printf("size is %ld\n", size);
    printf("writing to server\n");
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    free(send_req_str);
    printf("opening client pipe\n");
    // open client pipe to read
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    close(pipe_client);
    printf("return value from tfs_open is %d\n", ret_val);
    if (ret_val == -1){
        printf("error with tfs_open ocurred\n");
        return -1;
    }
    return 0;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    size_t size;
    int ret_val;
    printf("entered tfs_close\n");
    void *send_req_str = send_args( (char) TFS_OP_CODE_CLOSE, NULL, active_session_id, -1, fhandle, 0);
    printf("after creating request str\n");
    size = ((size_t *) send_req_str)[0];
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    printf("written to server\n");
    free(send_req_str);
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    close(pipe_client);
    printf("return value from tfs_close is %d\n", ret_val);
    if (ret_val == -1){
        printf("error with tfs_close ocurred\n");
        return -1;
    }
    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    size_t size;
    int ret_val;
    void *send_req_str = send_args( (char) TFS_OP_CODE_WRITE, buffer, active_session_id, -1, fhandle, len);
    size = ((size_t *) send_req_str)[0];
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    free(send_req_str);
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &ret_val, sizeof(int)) < 0) {
        return -1;
    }
    close(pipe_client);
    printf("return value from tfs_write is %d\n", ret_val);
    if (ret_val == -1){
        printf("error with tfs_write ocurred\n");
        return -1;
    }
    return ret_val;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    size_t size;
    int ret_val, nOfBytes;
    void *send_req_str = send_args( (char) TFS_OP_CODE_READ, NULL, active_session_id, -1, fhandle, len);
    size = ((size_t *) send_req_str)[0];
    if (write(pipe_server, send_req_str+sizeof(size_t), size) < 0) {
        return -1;
    }
    pipe_client = open(_client_pipe_path, O_RDONLY);
    if (pipe_client == -1) {
        return -1;
    }
    if (read(pipe_client, &nOfBytes, sizeof(int)) < 0) {
        return -1;
    }
    if (nOfBytes != -1){
        ret_val = read(pipe_client, buffer, nOfBytes);
    }
    close(pipe_client);
    return nOfBytes;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}

void *send_args(char opCode, void const *name, int session_id, int  flags, int fhandle, size_t len){
    size_t str_len=0, pipe_buf_size;
    if (name != NULL){
        str_len = strlen(name)+1;
    }
    size_t actual_size = 0;
    pipe_buf_size = (size_t) (sizeof(size_t) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + 40 + sizeof(int)); 
    void *request_msg = malloc(pipe_buf_size);
    void *ptr = request_msg;
    // put number of bytes allocated in the beginning of the pointer

    // leave space for actual size written
    request_msg += sizeof(size_t);
    
    // concatenate opCode
    //printf("writing opCode of %d\n", opCode);
    memcpy(request_msg, &opCode, sizeof(char));
    request_msg += sizeof(char);
    actual_size += sizeof(char);
    // concatenate session_id if necessary
    if (session_id != -1){
        //printf("writing session_id of %d\n", session_id);
        memcpy(request_msg, &session_id, sizeof(int));
        request_msg += sizeof(int);
        actual_size += sizeof(int);
    }
    
    // concatenate fhandle if necessary
    if (fhandle != -1){
        //printf("writing fhandle of %d\n", fhandle);
        memcpy(request_msg, &fhandle, sizeof(int));
        request_msg += sizeof(int);
        actual_size += sizeof(int);
    }
    if (len != 0){
        //printf("writing len of %ld\n", len);
        memcpy(request_msg, &len, sizeof(size_t));
        request_msg += sizeof(size_t);
        actual_size += sizeof(size_t);
    }
    if (name != NULL){
        //printf("writing name of %s\n", name);
        memcpy(request_msg, name, str_len);
        request_msg += 40;
        actual_size += 40;
    }
    if (flags != -1){
        //printf("writing flag of %d\n", flags);
        memcpy(request_msg, &flags, sizeof(int));
        request_msg += sizeof(int);
        actual_size += sizeof(int);
    }
    memcpy(ptr, &actual_size, sizeof(size_t));
    char eof = '\0';
    // memcpy(request_msg, &eof, sizeof(char));
    memcpy(ptr+pipe_buf_size-1, &eof, sizeof(char));
    return ptr;
}
