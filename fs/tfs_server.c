#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

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
    int pipe_server = open(pipename, O_RDONLY);

    if (pipe_server < 0) {
        exit(1);
    }

    int pipe_client;
    int session_id = 0;
    char *client_pipe_path;
    while(true) {
        char opCode;
        if (read(pipename,opCode,sizeof(char)) < 0) {
            return -1;
        }
        if (opCode == TFS_OP_CODE_MOUNT) {
            char readed[40];
            if (read(pipename,readed,40)<0) {
                return -1;
            }
            pipe_client = open(client_pipe_path, O_WRONLY);
            if (pipe_client == -1) {
                return -1;
            }
            if (write(pipe_client, &session_id, 1) < 0) {
                return -1;
            }
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            int session_id_r;
            if (read(pipename,&session_id_r,1)<0) {
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
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // read (int) fhandle
            if (read(pipe_server, &fhandle, sizeof(int)) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // read (size_t) len
            if (read(pipe_server, &len, sizeof(size_t)) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            readed = (char *) malloc(len);
            // chamar tfs_read do operations
            if (tfs_read(fhandle, readed, len) != len){
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // devolver info sobre como correu esta operação ao cliente
            // TODO
            write(pipe_client, 0, sizeof(int));
            return 0;
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            int session_id_atual, fhandle;
            size_t len;
            // read (int) session_id
            if (read(pipe_server, &session_id_atual, sizeof(int)) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // read (int) fhandle
            if (read(pipe_server, &fhandle, sizeof(int)) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // read (size_t) len
            if (read(pipe_server, &len, sizeof(size_t)) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            char buf [len];
            // read char [len] buffer
            if (read(pipe_server, buf, len) < 0) {
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // chamar tfs_write
            if (tfs_write(fhandle, buf, len) != len){
                write(pipe_client, -1, sizeof(int));
                return 0;
            }
            // devolver info sobre como correu esta operação ao cliente
            // TODO
            write(pipe_client, 0, sizeof(int));
            return 0;
        }
    }

    return 0;
}