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
char *read_name(int pipe_server);
int read_int(int pipe_server, int pipe_client);
size_t read_size_t(int pipe_server, int pipe_client);
int clean_pipe(int pipe_server);

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
    char *client_pipe_path;
    while(true) {
        char opCode;
        //read(pipe_server, &opCode, sizeof(char));
        
        if (read(pipe_server, &opCode, sizeof(char)) < 0) {
            return -1;
        }
        printf("op code is %d\n", (int) opCode);
        if (opCode == TFS_OP_CODE_MOUNT) {
            printf("opCode is mount\n");
            // read and set client pipe path
            client_pipe_path = read_name(pipe_server);
            /*
            clean_pipe(pipe_server);
            close(pipe_server);
            pipe_server = open(pipename, O_RDONLY);
            */
            // set session_id for this new session
            session_id = session_id_c;
           
            // opening client pipe and sending client new session id
            pipe_client = open(client_pipe_path, O_WRONLY);
            if (write(pipe_client, &session_id, sizeof(int)) < 0) {
                return -1;
            }
            printf("mount: written session id to client\n");
            session_id_c++;
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            printf("opCode is unmount\n");
            for (int i = 0; i < 40; i++){
                char c;
                read(pipe_server, &c, sizeof(char));
                if (c==EOF){
                    return 0;
                }
            }
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
            printf("getting out of unmount\n");
        }
        else if (opCode == TFS_OP_CODE_READ){
            printf("opCode is read\n");
            int session_id_atual, fhandle;
            size_t len;
            char *readed;

            // read (int) session id
            session_id_atual = read_int(pipe_server, pipe_client);
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);
            readed = read_name(pipe_server);
            // devolver info sobre como correu esta operação ao cliente
            pipe_client = open(client_pipe_path, O_WRONLY);
            ret_val = tfs_read(fhandle, readed, len);
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            close(pipe_client);
            return 0;
        }
        else if (opCode == TFS_OP_CODE_OPEN){
            printf("opCode is open\n");
            int session_id_atual, flags;
            char *name_ptr;
            
            // opening client pipe
            pipe_client = open(client_pipe_path, O_WRONLY);

            // read info from pipe
            session_id_atual = read_int(pipe_server, pipe_client);
            name_ptr = read_name(pipe_server);     
            flags = read_int(pipe_server, pipe_client);

            // perform operation
            ret_val = tfs_open(name_ptr, flags);

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            // closing client pipe
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            printf("opCode is write\n");
            int session_id_atual, fhandle;
            size_t len;
            char *name_ptr;
            
            // opening client pipe
            pipe_client = open(client_pipe_path, O_WRONLY);
            printf("reading from pipe\n");
            // read info from pipe
            session_id_atual = read_int(pipe_server, pipe_client);
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);
            name_ptr = read_name(pipe_server);     
            printf("session is is %d, fhandle is %d and len is %ld and name is %s\n", session_id_atual, fhandle, len, name_ptr);
            // perform operation
            ret_val = tfs_write(fhandle, name_ptr, len);
            
            if (ret_val != strlen(name_ptr)){
                ret_val = -1;
            }
            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            // closing client pipe
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_CLOSE){
            printf("opCode is close\n");
            int session_id_atual, flags;
            char *name_ptr;
            
            // opening client pipe
            pipe_client = open(client_pipe_path, O_WRONLY);

            // read info from pipe
            session_id_atual = read_int(pipe_server, pipe_client);
            name_ptr = read_name(pipe_server);     
            flags = read_int(pipe_server, pipe_client);

            // perform operation
            ret_val = tfs_open(name_ptr, flags);

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            // closing client pipe
            close(pipe_client);
        }
    }

    return 0;
}

char *read_name(int pipe_server){
    char readed[40];
    char *name;
    if (read(pipe_server, readed, 40) < 0) {
        return -1;
    }
    name = malloc(strlen(readed)+1);
    memcpy(name, readed, strlen(readed)+1);
    return name;
}

int read_int(int pipe_server, int pipe_client){
    int val, ret_val;
    if (read(pipe_server, &val, sizeof(int)) < 0) {
        ret_val = -1;
        if (write(pipe_client, &ret_val, sizeof(int)) < 0){
            return -1;
        }
        return -1;
    }
    return val;
}

size_t read_size_t(int pipe_server, int pipe_client){
    size_t len;
    int ret_val;
    if (read(pipe_server, &len, sizeof(size_t)) < 0) {
        ret_val = -1;
        if (write(pipe_client, &ret_val, sizeof(int)) < 0){
            return 0;
        }
        return 0;
    }
    return len;
}

int clean_pipe(int pipe_server){
    char c = '0';
    /*
    while (c != '\0'){
        read(pipe_server, &c, sizeof(char));
    }
    */
    printf("before while\n");
    while (c!='\0' && read(pipe_server, &c, sizeof(char)) > 0);
    printf("after while\n");
    return 0;
}