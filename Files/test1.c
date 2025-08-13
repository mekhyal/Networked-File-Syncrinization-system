#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/socket.h>

#define DIR_NAME "ServerDir"
#define BASE_PATH "/home/aziz32/223Project/Files/" DIR_NAME
#define PORT 4590

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n)
{
    size_t left = n;
    char *p = buf;
    while (left > 0)
    {
        ssize_t r = read(fd, p, left);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (r == 0)
        {
            return 0;
        }
        left -= r;
        p += r;
    }
    return n;
}

int open_listenfd(int port)
{
        //Create socket for the server and return to variable serverfd 
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0)
    {
        perror("Socket Failed..");
        exit(1);
    }

    /*First Fill the server_addr to avoid garbage data for safety
    /Specify IPv4 addressing
    Accept any IP address
    PORT is your server port number.*/
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //bind() is the function that tells the OS:
    //“Hey, I want to listen for connections on this IP and this port.”
    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("There is error of using bind...");
        close(serverfd);
        exit(1);
    }

    //Listen for incoming connections
    if (listen(serverfd, 5) < 0)
    {
        perror("Listen Failed.");
        close(serverfd);
        exit(1);
    }

    return serverfd;
}


//if Directory exist does not create if not will be create for client
//stat helps you to check the file or directory exist or not 
void createDir(int clientfd,char *parent_path,char *finalpath,size_t finalpath_len)
{
    // Received from client (by socket client) the length of the directory name
    uint32_t len;
    if (readn(clientfd, &len, sizeof (len)) != sizeof (len))
    {
        perror("Failed to read length");
        return;
    }
    len = ntohl(len);

    // Received from client (by socket client) the directory name
    char name[len + 1]; // +1 for the null terminator
    if (readn(clientfd, name, len) != (ssize_t)len)
    {
        perror("Failed to read directory name");
        return;
    }
    name[len] = '\0';

    // build full path under parent_path
    snprintf(finalpath, finalpath_len, "%s/%s", parent_path, name);

    //The return value is 0 if the operation is successful(exist), or -1 on failure
    // Check if directory exists or create it
    struct stat st;
    if (stat(finalpath, &st) == 0 && S_ISDIR(st.st_mode))
    {
        printf("Directory already exists: %s\n", finalpath);
    }
    else if (mkdir(finalpath, 0755) == 0)
    {
        printf("Created directory: %s\n", finalpath);
    }
    else
    {
        perror("Error creating directory");
    }
}


//Use uint32_t to make sure you use 4-bytes regardless for all platform
void savefile(int clientfd,char *path)
{
    // 1.filename length
    uint32_t fnlen;
    if (readn(clientfd, &fnlen, sizeof(fnlen)) != sizeof(fnlen))
    {
        perror("Failed to read filename length");
        return;
    }
    //convert byte order from big Endian to little Endian
    fnlen = ntohl(fnlen);

    //Received from client (by socket client) the filename
    char filename[fnlen + 1];
    if (readn(clientfd, filename, fnlen) != (ssize_t)fnlen)
    {
        perror("Failed to read filename");
        return;
    }
    filename[fnlen] = '\0';

    //Received from client (by socket client) the file size
    uint32_t fsize;
    if (readn(clientfd, &fsize, sizeof(fsize)) != sizeof(fsize))
    {
        perror("Failed to read filename length");
        return;
    }
    //convert byte order from big Endian to little Endian
    fsize = ntohl(fsize);


    //construct full path  
    char fullpath[512];
    snprintf(fullpath, sizeof fullpath, "%s/%s", path, filename);

    //Always use "rb" when reading files to send over sockets it's sending any file reliably
    //open file for writing
    FILE *fptr = fopen(fullpath, "wb");
    if (!fptr)
    {
        perror("Failed to open file for writing.");
        return;
    }

    //Read file content
    size_t received = 0;
    char buf[1024];
    while (received < fsize)
    {
        //Read the minimum between how much is left and the buffer size.
        size_t remaining = fsize - received;
        size_t to_read = (remaining < sizeof(buf)) ? remaining : sizeof(buf);    

        //Try to read exactly to_read bytes from the client socket into buf.
        ssize_t r = readn(clientfd, buf, to_read);
        if (r <= 0)
        {
            perror("Failed to read file data");
            break;
        }
        fwrite(buf, 1, r, fptr);
        received += r;
    }
    fclose(fptr);
    printf("Received file: %s (%u bytes)\n", filename, fsize);
}


void subDirectory(int clientfd, char *base_path)
{
    while (1)
    {
        uint8_t flag;
        if (readn(clientfd, &flag, 1) != 1)
        {
            perror("Failed to read flag.");
            return;
        }
        if (flag == 0)
        {
            printf("Client finished sending files.\n");
            return;
        }
        else if (flag == 1)
        {
            // regular file
            savefile(clientfd, base_path);
        }
        else if (flag == 2)
        {
            char subpath[512];
            // reuse createDir to read name and mkdir
            createDir(clientfd, base_path, subpath, sizeof(subpath));
            // recursively into new subDirectory
            subDirectory(clientfd, subpath);
        }
        else
        {
            fprintf(stderr, "Unknown flag %u\n", flag);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int serverfd = open_listenfd(port);
    if(serverfd < 0){
        perror("Server setup failed.\n");
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    struct stat st;
    // The return value is 0 if the operation is successful(exist), or -1 on failure
    if (stat(DIR_NAME, &st) == 0){
        printf("Directory already exists.\n");
    }
    else{
        if (mkdir(DIR_NAME, 0755) == 0){
            printf("Empty directory created successfully.\n");
        }
        else{
            perror("Error creating directory");
            return 1;
        }
    }

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof (client_addr);
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
    createDir(clientfd, BASE_PATH, topath, sizeof(topath));
    printf("Syncing under: %s\n", topath);

    // handle all files & subdirectories recursively
    subDirectory(clientfd, topath);

    close(clientfd);
    close(serverfd);
    return 0;
}
