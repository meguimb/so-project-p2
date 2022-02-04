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
#include <signal.h>

#define S 16
#define FREE 0
#define TAKEN 1

struct client_info {
    int session_id;
    int client_pipe;
};
typedef struct client_info ClientInfo;

struct input_buffer {
    char opCode;
    ClientInfo *client;
    int session_id;
    char *name;
    int flags;
    int fhandle;
    size_t len;
    bool work;
};
typedef struct input_buffer InputBuffer;

char *read_name(int pipe_server);
int read_int(int pipe_server, int pipe_client);
size_t read_size_t(int pipe_server, int pipe_client);
int clean_pipe(int pipe_server);
void *execute(void *args);
int error_check(int client_pipe);
int find_free_session(int table [S]);
int free_mem(InputBuffer *consumer_inputs [S], ClientInfo *clients [S]);

// Global Variables
static pthread_mutex_t buffers_mutexes [S];
static pthread_cond_t can_consume [S];
static pthread_cond_t can_produce [S];
static pthread_t tasks[S];
static pthread_mutex_t session_table_mutex;
static int open_server = 1;
static int free_sessions[S] = {FREE};


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

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
    int ret_val;
    int pipe_server = open(pipename, O_RDONLY);

    if (pipe_server < 0) {
        exit(1);
    }
    /* create loop to create S pthreads that run the function work/execute with 
    the buffer[i] as the parameter */

    // create S buffers
    for (int i = 0; i < S; i++){
        consumer_inputs[i] = (InputBuffer *) malloc(sizeof(InputBuffer));
        consumer_inputs[i]->work = false;
        consumer_inputs[i]->session_id = i;
        clients[i] = NULL;
        pthread_create(&tasks[i], NULL, execute, consumer_inputs[i]);
        pthread_cond_init(&can_produce[i], NULL);
        pthread_cond_init(&can_consume[i], NULL);
        pthread_mutex_init(&buffers_mutexes[i], NULL);
    }
    pthread_mutex_init(&session_table_mutex, NULL);

    while(true && open_server==1) {
        char opCode;
        if (read(pipe_server, &opCode, sizeof(char)) < 0) {
            return -1;
        }
        if (opCode == TFS_OP_CODE_MOUNT) {
            int session_id;
            char *client_pipe_path;
            
            session_id = find_free_session(free_sessions);
            client_pipe_path = read_name(pipe_server);

            // if all sessions are taken
            if (session_id == -1){
                int client_pipe = open(client_pipe_path, O_WRONLY);
                if (write(client_pipe, &session_id, sizeof(int)) < 0) {
                    error_check(client_pipe);
                }
                else{
                    close(client_pipe);
                }
            }
            else{
                pthread_mutex_lock(&buffers_mutexes[session_id]);
                clients[session_id] = malloc(sizeof(ClientInfo *));
                ClientInfo *c = clients[session_id];
                c->session_id = session_id;
                // opening client pipe and sending client new session id
                c->client_pipe = open(client_pipe_path, O_WRONLY);
                if (write(c->client_pipe, &c->session_id, sizeof(int)) < 0) {
                    error_check(c->client_pipe);
                }
                pthread_mutex_unlock(&buffers_mutexes[session_id]);
            }
        }
        else if (opCode == TFS_OP_CODE_UNMOUNT) {
            int pipe_client, session_id = read_int(pipe_server, -1);
            ret_val = 1;
            pthread_mutex_lock(&session_table_mutex);
            free_sessions[session_id] = FREE;
            pthread_mutex_unlock(&session_table_mutex);

            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;
            free(clients[session_id]);
            if (write(pipe_client, &ret_val, sizeof(int)) < 0){
                close(pipe_client);
                return -1;
            }
            close(pipe_client);
        }
        else if (opCode == TFS_OP_CODE_READ){
            int session_id, pipe_client;

            // read (int) session id
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);

            // get info of this session
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->fhandle = read_int(pipe_server, pipe_client);;
            buf->len = read_size_t(pipe_server, pipe_client);;
            buf->opCode = TFS_OP_CODE_READ;
            buf->work = true;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
        }
        else if (opCode == TFS_OP_CODE_OPEN){
            int session_id, pipe_client;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            ClientInfo *c = clients[session_id];

            // opening client pipe
            pipe_client = c->client_pipe;

            // put parameters on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->name = read_name(pipe_server);
            buf->flags = read_int(pipe_server, pipe_client);
            buf->opCode = TFS_OP_CODE_OPEN;
            buf->work = true;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            int session_id, pipe_client;
            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);
            // get client pipe 
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;   
            // read parameters and put them on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->fhandle = read_int(pipe_server, pipe_client);
            buf->len = read_size_t(pipe_server, pipe_client);
            buf->name = read_name(pipe_server);    
            buf->opCode = TFS_OP_CODE_WRITE;
            buf->work = true;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
        }
        else if (opCode == TFS_OP_CODE_CLOSE){
            int session_id, pipe_client;

            // read info from pipe
            session_id = read_int(pipe_server, -1);
            pthread_mutex_lock(&buffers_mutexes[session_id]);

            // getting client pipe
            ClientInfo *c = clients[session_id];
            pipe_client = c->client_pipe;

            // read parameters and put them on buffer
            InputBuffer *buf = consumer_inputs[session_id];
            buf->client = c;
            buf->fhandle = read_int(pipe_server, pipe_client);
            buf->opCode = TFS_OP_CODE_CLOSE;
            buf->work = true;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);
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
            buf->work = true;

            // pthread signal
            pthread_cond_signal(&can_consume[session_id]);
            pthread_mutex_unlock(&buffers_mutexes[session_id]);

            // free'ing all memory allocated
            free_mem(consumer_inputs, clients);

            // closing client pipe
            close(pipe_client);
            close(pipe_server);
            printf("Shutting down TecnicoFS server.\n");
            return 0;
        }
    }
    close(pipe_server);
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
    char opCode;
    char *text_read;
    int ret_val, session_id = buf->session_id;
    while(true){
        pthread_mutex_lock(&buffers_mutexes[session_id]);
        while (!buf->work){
            pthread_cond_wait(&can_consume[session_id], &buffers_mutexes[session_id]);
        }
        opCode = buf->opCode;
        session_id = buf->client->session_id;
        if (opCode == TFS_OP_CODE_OPEN){
            ret_val = tfs_open(buf->name, buf->flags);
            // send return code to client
            if (write(buf->client->client_pipe, &ret_val, sizeof(int)) < 0){
                buf->work = false;
               if (error_check(buf->client->client_pipe) == -2){
                    return NULL;
                }
            }
        }
        else if (opCode == TFS_OP_CODE_CLOSE){
            ret_val = tfs_close(buf->fhandle);

            // send return code to client
            if (write(buf->client->client_pipe, &ret_val, sizeof(int)) < 0){
                buf->work = false;
                if (error_check(buf->client->client_pipe) == -2){
                    return NULL;
                }
            }
        }
        else if (opCode == TFS_OP_CODE_READ){
            text_read = malloc(buf->len);
            ret_val = (int) tfs_read(buf->fhandle, text_read, buf->len);

            // devolver info sobre como correu esta operação ao cliente
            if (ret_val != strlen(text_read)){
                ret_val = -1;
            }

            if (write(buf->client->client_pipe, &ret_val, sizeof(int)) > 0 && ret_val != -1){
                if (write(buf->client->client_pipe, text_read, (size_t) ret_val) < 0){
                    buf->work = false;
                    if (error_check(buf->client->client_pipe) == -2){
                        return NULL;
                    }
                }
            }
            else{
                buf->work = false;
                if (error_check(buf->client->client_pipe) == -2){
                    return NULL;
                }
            }
        }
        else if (opCode == TFS_OP_CODE_WRITE){
            ret_val = (int) tfs_write(buf->fhandle, buf->name, buf->len);
            if (ret_val != strlen(buf->name)){
                ret_val = -1;
            }
            // send return code to client
            if (write(buf->client->client_pipe, &ret_val, sizeof(int)) < 0){
                buf->work = false;
                if (error_check(buf->client->client_pipe) == -2){
                    return NULL;
                }
            }
        }
        else if (opCode == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            ret_val = tfs_destroy_after_all_closed();

            // send return code to client
            if (write(buf->client->client_pipe, &ret_val, sizeof(int)) < 0){
                buf->work = false;
                if (error_check(buf->client->client_pipe) == -2){
                    
                    return NULL;
                }
            }
        }
        buf->work = false;
        // pthread_cond_signal(&can_produce[session_id]);
        pthread_mutex_unlock(&buffers_mutexes[session_id]);
    }
}

// se nao for possivel escrever, descobrir qual erro está a acontecer
// se for do pipe, fechar pipe
// se for do signal, tentar outra vez

int error_check(int client_pipe){
    // verificar se é erro do pipe
    if (errno == EPIPE){
        close(client_pipe);
        return -1;
    }
    // verificar se é erro do sinal
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 0;
    }
    open_server = 0;
    return -2;
}

int find_free_session(int table [S]){
    pthread_mutex_lock(&session_table_mutex);
    for (int i = 0; i < S; i++){
        if (table[i] == FREE){
            table[i] = TAKEN;
            pthread_mutex_unlock(&session_table_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&session_table_mutex);
    return -1;
}

int free_mem(InputBuffer *consumer_inputs [S], ClientInfo *clients [S]){
    for (int i = 0; i < S; i++){
        free(consumer_inputs[i]);
        if (clients[i] != NULL){
            free(clients[i]);
        }
    }
    return 0;
}