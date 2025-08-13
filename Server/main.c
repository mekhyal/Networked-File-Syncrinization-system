#include "server.h"


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int serverfd = open_listenfd(atoi(argv[1]));
    if (serverfd < 0)
    {
        perror("Server setup failed.\n");
        return 1;
    }

    printf("Server listening on port %d...\n", atoi(argv[1]));

    struct stat st;
    if (stat(BASE_PATH, &st) == 0)
    {
        printf("Directory already exists: %s\n", BASE_PATH);
    }
    else if (mkdir(BASE_PATH, 0755) == 0)
    {
        printf("Empty directory created successfully: %s\n", BASE_PATH);
    }
    else
    {
        perror("Error creating directory");
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof client_addr;
    int clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &addrlen);
    if (clientfd < 0)
    {
        perror("Accept Failed");
        close(serverfd);
        return 1;
    }
    printf("Client connected.\n");

    // read and create the top-level directory under BASE_PATH
    char topath[512];
    if(createDir(clientfd, BASE_PATH, topath, sizeof topath) == -1){
        perror("Failed to create directory");
        return 1;
    }
    printf("Syncing under: %s\n", topath);

    // handle all files & subdirectories recursively
    if(saveDir(clientfd, topath) == -1){
        perror("Failed to create directory");
        return 1;
    }
    

    close(clientfd);
    close(serverfd);
    return 0;
}
