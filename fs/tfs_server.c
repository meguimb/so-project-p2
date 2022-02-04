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
#include <pthread.h>

#define S 16

struct client_info {
    int session_id;
    int client_pipe;
};
typedef struct client_info ClientInfo;

struct input_buffer {
    char opCode;
    ClientInfo *client;
    char *name;
    int flags;
    int fhandle;
    size_t len;
};
typedef struct input_buffer InputBuffer;

char *read_name(int pipe_server);
int read_int(int pipe_server, int pipe_client);
size_t read_size_t(int pipe_server, int pipe_client);
int clean_pipe(int pipe_server);

// Global Variables
pthread_mutex_t buffers_mutexes [S];
pthread_cond_t can_consume [S];
pthread_cond_t can_produce [S];

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    // PARTE 2 ETAPA 2
    // no while vamos receber u
    // array de S+1 tarefas, 1 recetora, S trabalhadoras, cada uma associada a um session_id
    // as tarefas trabalhadoras servem os pedidos que a tarefa recetora recebe

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
    InputBuffer *consumer_inputs [S];
    int session_id_c = 0;
    int ret_val;
    int pipe_server = open(pipename, O_RDONLY);
    char *client_pipe_path;

    if (pipe_server < 0) {
        exit(1);
    }
    /* create loop to create S pthreads that run the function work/execute with 
    the buffer[i] as the parameter */

    // create S buffers
    for (int i = 0; i < S; i++){
        consumer_inputs[i] = (InputBuffer *) malloc(sizeof(InputBuffer));
        pthread_cond_init(&can_produce[i], NULL);
        pthread_cond_init(&can_consume[i], NULL);
        pthread_mutex_init(&buffers_mutexes[i], NULL);
    }

    while(true) {
        char opCode;
        //read(pipe_server, &opCode, sizeof(char));
        
        if (read(pipe_server, &opCode, sizeof(char)) < 0) {
            return -1;
        }
        if (opCode == TFS_OP_CODE_MOUNT) {
            int session_id = session_id_c;
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            // read and set client pipe path
            client_pipe_path = read_name(pipe_server);
            if (session_id >= S){
                return -1;
            }
            clients[session_id] = malloc(sizeof(ClientInfo *));
            ClientInfo *c = clients[session_id];
            c->session_id = session_id;
            // opening client pipe and sending client new session id
            c->client_pipe = open(client_pipe_path, O_WRONLY);
            if (write(c->client_pipe, &c->session_id, sizeof(int)) < 0) {
                return -1;
            }
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
            session_id_c++;
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            int pipe_client, session_id = read_int(pipe_server, -1);
            ret_val = 1;
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;
            
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_READ){
            int session_id, fhandle, pipe_client;
            size_t len;
            char *readed;

            // read (int) session id
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);

            // get info of this session
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;
            // read other request parameters
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->fhandle = fhandle;
            buf->len = len;
            buf->opCode = TFS_OP_CODE_READ;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);

            // devolver info sobre como correu esta operação ao cliente
            readed = malloc(len);
            ret_val = (int) tfs_read(fhandle, readed, len);
            if (ret_val != strlen(readed)){
                ret_val = -1;
            }

            write(pipe_client, &ret_val, sizeof(int));
            if (ret_val != -1){
                write(pipe_client, readed, (size_t) ret_val);
            }
        }
        else if (opCode == TFS_OP_CODE_OPEN){
            int session_id, flags, pipe_client;
            char *name_ptr;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            ClientInfo *c = clients[session_id];

            // opening client pipe
            pipe_client = c->client_pipe;
            name_ptr = read_name(pipe_server);     
            flags = read_int(pipe_server, pipe_client);

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->name = name_ptr;
            buf->flags = flags;
            buf->opCode = TFS_OP_CODE_OPEN;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);

            // perform operation
            ret_val = tfs_open(name_ptr, flags);

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                return -1;
            }
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            int session_id, fhandle, pipe_client;
            size_t len;
            char *name_ptr;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            ClientInfo *c = clients[session_id];

            // open client pipe 
            pipe_client = c->client_pipe;

            // read rest of info
            fhandle = read_int(pipe_server, pipe_client);
            len = read_size_t(pipe_server, pipe_client);
            name_ptr = read_name(pipe_server);    

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->name = name_ptr;
            buf->fhandle = fhandle;
            buf->len = len;
            buf->opCode = TFS_OP_CODE_WRITE;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
        
            // perform operation
            ret_val = (int) tfs_write(fhandle, name_ptr, len);
            
            if (ret_val != strlen(name_ptr)){
                ret_val = -1;
            }

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                return -1;
            }
            // closing client pipe
        }
        else if (opCode == TFS_OP_CODE_CLOSE){
            int session_id, fhandle, pipe_client;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);

            // getting client pipe
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;

            fhandle = read_int(pipe_server, pipe_client);

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->fhandle = fhandle;
            buf->opCode = TFS_OP_CODE_CLOSE;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);

            // perform operation
            ret_val = tfs_close(fhandle);

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                return -1;
            }
        }
        else if (opCode == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            int session_id, pipe_client;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            // geting session info
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->opCode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);

            // perform operation
            ret_val = tfs_destroy_after_all_closed();

            // send return code to client
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
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

void *execute(void *args){
    InputBuffer *buf = (InputBuffer *) args;
    char opCode = buf->opCode;
    int session_id = buf->client->session_id;
    pthread_mutex_lock(&buffers_mutexes[session_id]);
    if (opCode == TFS_OP_CODE_OPEN){
        tfs_open(buf->name, buf->flags);
    }
    else if (opCode == TFS_OP_CODE_CLOSE){
        tfs_close(buf->fhandle);
    }
    else if (opCode == TFS_OP_CODE_READ){
        tfs_read(buf->fhandle, buf->name, buf->len);
    }
    else if (opCode == TFS_OP_CODE_WRITE){
        tfs_write(buf->fhandle, buf->name, buf->len);
    }
    else if (opCode == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
        tfs_destroy_after_all_closed();
    }
    pthread_cond_signal(&can_produce[session_id]);
    pthread_mutex_lock(&buffers_mutexes[session_id]);
    return NULL;
}