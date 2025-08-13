#include "server.h"

int open_listenfd(int port)
{
    // Create socket for the server
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0){
        perror("Socket Failed..");
        return -1;
    }

    // Specify IPv4 addressing, accept any IP, set port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind() tells the OS to listen on this IP and port
    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("There is error of using bind...");
        close(serverfd);
        return -1;
    }

    // Listen for incoming connections
    if (listen(serverfd, 5) < 0){
        perror("Listen Failed.");
        close(serverfd);
        return -1;
    }

    return serverfd;
}