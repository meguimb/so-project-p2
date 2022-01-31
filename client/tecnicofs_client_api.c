#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int session_id;
int pipe;
int serverpipe;
char*pipe_path;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    if (unlink(client_pipe_path)!=0) {
        return -1;
    }
    if (mkfifo(client_pipe_path,0640)!=0) {
        return -1;
    }
    serverpipe = open(server_pipe_path, O_WRONLY);
    if (serverpipe == -1) {
        return -1;
    }
    strcpy(pipe_path,client_pipe_path);
    char* msg = TFS_OP_CODE_MOUNT;
    strcat(msg," | ");
    strcat(msg,client_pipe_path);
    size_t len = strlen(msg);
    if (write(server,msg,len) < 0) {
        return -1;
    }
    pipe = open(client_pipe_path, O_RDONLY);
    if (pipe == -1) {
        return -1;
    }
    if (read(client,&session_id,1) < 0) {
        return -1;
    }
    return 0;
}

int tfs_unmount() {
    char* msg = TFS_OP_CODE_UNMOUNT;
    strcat(msg, " | ");
    strcat(msg,session_id);
    if (write(server,msg,len) < 0) {
        return -1;
    }
    if (close(pipe) < 0) {
        return -1;
    }
    if (close(serverpipe) < 0) {
        return -1;
    }
    if (unlink(pipe_path)!=0) {
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
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
