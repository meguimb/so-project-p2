#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */

    if (unlink(pipename)!= 0) {
        exit(1);
    }

    if (mkfifo(pipename, 0640)!=0) {
        exit(1);
    }

    if (tfs_init()!=0) {
        exit(1);
    }

    int pipe = open(pipename, O_RDONLY);

    if (pipe < 0) {
        exit(1);
    }

    int clientpipe;
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
            int clientpipe = open(client_pipe_path, O_WRONLY);
            if (clientpipe == -1) {
                return -1;
            }
            if (write(clientpipe, &session_id, 1) < 0) {
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
            if (close(clientpipe) < 0) {
                return -1;
            }
        }
        else if (opCode == TFS_OP_CODE_READ){
            // read (int) session id => 6 bytes

            // read (int) fhandle => 6 bytes

            // read (size_t) len => 8 bytes

        }
        else if (opCode == TFS_OP_CODE_WRITE){
            // read (int) session_id

            // read (int) fhandle

            // read (size_t) len

            // read char [len] buffer
        }
    }

    return 0;
}