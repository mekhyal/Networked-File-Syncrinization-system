#include "client.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <local_directory> [server_directory_name]\n", argv[0]);
        exit(1);
    }

    const char *localDir = argv[1];

    // Use provided serverDir name or extract the last component of localDir
    const char *serverDir = (argc > 2) ? argv[2] :
        (strrchr(localDir, '/') ? strrchr(localDir, '/') + 1 : localDir);

    // Connect to the server
    int sockFd = open_clientfd();
    printf("Client connected to server...\n");

    // Send the directory name to the server
    dirToSocket(sockFd, serverDir);

    // Begin recursive traversal and file sync
    traversDir(sockFd, localDir);

    // Clean up
    close(sockFd);
    printf("Client finished sending files.\n");

    return 0;
}

