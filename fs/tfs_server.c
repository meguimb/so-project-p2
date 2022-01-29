#include "operations.h"

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

    return 0;
}