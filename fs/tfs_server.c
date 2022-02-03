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

#define S 16

struct client_info {
    int session_id;
    char *client_path;
};
typedef struct client_info ClientInfo;

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

    
    // API -> write no pipe_server e read do client_server
    // SERVER -> read do pipe_server e write no client_server
    if (unlink(pipename) != 0 && errno != ENOENT) {
        exit(EXIT_FAILURE);
    }
    if (mkfifo(pipename, 0640) < 0) {
        exit(1);
    }
    
    if (tfs_init()!=0) {
        exit(1);
    }
    
    /* creating client struct array */
    ClientInfo *clients [S];
    int session_id_c = 0;
    int active_session;
    int ret_val;
    int pipe_server = open(pipename, O_RDONLY);
    int pipe_client = -1;
    char *client_pipe_path;

    if (pipe_server < 0) {
        exit(1);
    }

    while(true) {
        char opCode;
        //read(pipe_server, &opCode, sizeof(char));
        
        if (read(pipe_server, &opCode, sizeof(char)) < 0) {
            return -1;
        }
        if (opCode == TFS_OP_CODE_MOUNT) {
            // read and set client pipe path
            client_pipe_path = read_name(pipe_server);
            if (session_id_c >= S){
                return -1;
            }
            clients[session_id_c] = malloc(sizeof(ClientInfo *));
            ClientInfo *c = clients[session_id_c];
            c->session_id = session_id_c;
            //c->client_path = (char *) malloc(strlen(client_pipe_path)+1);
            //memcpy(c->client_path, client_pipe_path, strlen(client_pipe_path)+1);
            //c.client_path = client_pipe_path;
            c->client_path = client_pipe_path;
            // opening client pipe and sending client new session id
            pipe_client = open(c->client_path, O_WRONLY);
            if (write(pipe_client, &c->session_id, sizeof(int)) < 0) {
                return -1;
            }
            session_id_c++;
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            int session_id = read_int(pipe_server, pipe_client);
            ret_val = 1;
            ClientInfo *c = clients[session_id];
            pipe_client = open(c->client_path, O_WRONLY);
            if (pipe_client < 0){
                printf("%d\n", pipe_client);
            }
            
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_READ){
            int session_id, fhandle;
            size_t len;
            char *readed;

            // read (int) session id
            session_id = read_int(pipe_server, pipe_client);
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);
            ClientInfo *c = clients[session_id];
            // devolver info sobre como correu esta operação ao cliente
            pipe_client = open(c->client_path, O_WRONLY);
            readed = malloc(len);
            ret_val = (int) tfs_read(fhandle, readed, len);
            if (ret_val != strlen(readed)){
                ret_val = -1;
            }
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
            }
            if (ret_val != -1){
                if (write(pipe_client, readed, (size_t) ret_val) < 0){
                    close(pipe_client);
                }
            }
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_OPEN){
            int session_id, flags;
            char *name_ptr;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            ClientInfo *c = clients[session_id];

            // opening client pipe
            pipe_client = open(c->client_path, O_WRONLY);
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
            int session_id, fhandle;
            size_t len;
            char *name_ptr;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            ClientInfo *c = clients[session_id];

            // open client pipe 
            pipe_client = open(c->client_path, O_WRONLY);

            // read rest of info
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);
            name_ptr = read_name(pipe_server);     
            // perform operation
            ret_val = (int) tfs_write(fhandle, name_ptr, len);
            
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
            int session_id, fhandle;

            // read info from pipe
            session_id = read_int(pipe_server, pipe_client);

            // opening client pipe
            ClientInfo *c = clients[session_id];
            pipe_client = open(c->client_path, O_WRONLY);

            fhandle = read_int(pipe_server, pipe_client);

            // perform operation
            ret_val = tfs_close(fhandle);

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            // closing client pipe
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            int session_id;

            // read info from pipe
            session_id = read_int(pipe_server, pipe_client);

            // perform operation
            ret_val = tfs_destroy_after_all_closed();

            // opening client pipe
            ClientInfo *c = clients[session_id];
            pipe_client = open(c->client_path, O_WRONLY);
            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            printf("Shutting down TecnicoFS server.\n");
            // closing client pipe
            close(pipe_client);
            close(pipe_server);
            return 0;
        }
    }

    return 0;
}

char *read_name(int pipe_server){
    char readed[40];
    char *name;
    if (read(pipe_server, readed, 40) < 0) {
        return NULL;
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
    while (c!='\0' && read(pipe_server, &c, sizeof(char)) > 0);
    return 0;
}