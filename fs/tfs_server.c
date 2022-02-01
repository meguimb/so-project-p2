#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int session_id_c;

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    // API -> write no pipe_server e read do client_server
    // SERVER -> read do pipe_server e write no client_server
    if (unlink(pipename) != 0 && errno != ENOENT) {
        exit(EXIT_FAILURE);
    }
    if (mkfifo(pipename, 0640) < 0) {
        exit(1);
    }
    printf("before tfs_init\n");
    if (tfs_init()!=0) {
        exit(1);
    }
    printf("tfs server opening server pipe\n");
    session_id_c = 0;
    int session_id;
    int ret_val;
    int pipe_server = open(pipename, O_RDONLY);

    if (pipe_server < 0) {
        exit(1);
    }

    int pipe_client;
    char *client_pipe_path = (char *) malloc(40);
    while(true) {
        char opCode;
        //read(pipe_server, &opCode, sizeof(char));
        
        if (read(pipe_server, &opCode, sizeof(char)) < 0) {
            return -1;
        }
        if (opCode == TFS_OP_CODE_MOUNT) {
            char readed[40];
            if (read(pipe_server, readed, 40) < 0) {
                return -1;
            }
            memcpy(client_pipe_path, readed, strlen(readed)+1);
            client_pipe_path[strlen(readed)] = '\0';
            pipe_client = open(client_pipe_path, O_WRONLY);
            session_id = session_id_c;
            if (write(pipe_client, &session_id, sizeof(int)) < 0) {
                return -1;
            }
            session_id_c++;
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            int session_id_r;
            if (read(pipe_server, &session_id_r, sizeof(int)) < 0) {
                return -1;
            }
            if (session_id_r != session_id) {
                return -1;
            }
            if (close(pipe_client) < 0) {
                return -1;
            }
        }
        else if (opCode == TFS_OP_CODE_READ){
            int session_id_atual, fhandle;
            size_t len;
            char *readed;
            // read (int) session id
            if (read(pipe_server, &session_id_atual, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // read (int) fhandle
            if (read(pipe_server, &fhandle, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // read (size_t) len
            if (read(pipe_server, &len, sizeof(size_t)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            readed = (char *) malloc(len);
            // chamar tfs_read do operations
            if (tfs_read(fhandle, readed, len) != len){
                ret_val = -1;   
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // devolver info sobre como correu esta operação ao cliente
            // TODO
            ret_val = 0;
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                return -1;
            }
            return 0;
        }
        else if (opCode == TFS_OP_CODE_OPEN){
            printf("opCode OPEN\n");
            int session_id_atual, flags;
            char name [40];
            char *name_ptr;
            close(pipe_server);
            pipe_server = open(pipename, O_RDONLY);
            // read (int) session_id
            if (read(pipe_server, &session_id_atual, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // read char name [40]
            if (read(pipe_server, name, 40) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            size_t len = strlen(name);
            printf("name is: %s\n", name);
            name_ptr = malloc(len+1);
            memcpy(name_ptr, name, len+1);
            name_ptr[len] = '\0';
            // read flags
            if (read(pipe_server, &flags, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            printf("doing tfs_open\n");
            printf("session_id is %d name is %s and flag is %d\n", session_id, name_ptr, flags);
            ret_val = tfs_open(name_ptr, flags);
            printf("ret_val from tfs_open operations is %d\n", ret_val);
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                return -1;
            }
            printf("server: leaving tfs_open\n");
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            int session_id_atual, fhandle;
            size_t len;
            // read (int) session_id
            if (read(pipe_server, &session_id_atual, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // read (int) fhandle
            if (read(pipe_server, &fhandle, sizeof(int)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // read (size_t) len
            if (read(pipe_server, &len, sizeof(size_t)) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0 ){
                    return -1;
                }
                return 0;
            }
            char buf [len];
            // read char [len] buffer
            if (read(pipe_server, buf, len) < 0) {
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
            }
            // chamar tfs_write
            if (tfs_write(fhandle, buf, len) != len){
                ret_val = -1;
                if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                    return -1;
                }
                return 0;
            }
            // devolver info sobre como correu esta operação ao cliente
            // TODO
            if (write(pipe_client, 0, sizeof(int)) < 0 ){
                return -1;
            }
            return 0;
        }
    }

    return 0;
}