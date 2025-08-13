#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/socket.h>
#include <dirent.h>
#include <zlib.h>

#define DIR_NAME "ServerDir"
#define BASE_PATH "/home/aziz32/223Project/Files/" DIR_NAME
#define PORT 4590


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

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n){
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

ssize_t sendn(int fd, const void *buf, size_t n) {
  size_t sent = 0;
  const char *p = buf;
  while (sent < n) {
      ssize_t r = send(fd, p + sent, n - sent, 0);
      if (r < 0) {
          if (errno == EINTR) continue;
          perror("send");
          exit(1);
      }
      sent += r;
  }
  return sent;
}

int createDir(int clientfd,char *server_path, char *client_path , int client_path_len)
{
  // Received from client (by socket client) the length of the directory name
  uint32_t len;
  if (readn(clientfd, &len, sizeof (len)) != sizeof (len)){
    perror("Failed to read length");
    return -1;
  }
  len = ntohl(len);

  // Received from client (by socket client) the directory name
  char name[len + 1]; // +1 for the null terminator
  if (readn(clientfd, name, len) != (ssize_t)len){
    perror("Failed to read directory name");
    return -1;
  }
  name[len] = '\0';

  // build full path under parent_path
  snprintf(client_path, client_path_len, "%s/%s", server_path, name);

  //The return value is 0 if the operation is successful(exist), or -1 on failure
  // Check if directory exists or create it
  struct stat st;
  if (stat(client_path, &st) == 0 && S_ISDIR(st.st_mode))
    printf("Directory already exists: %s\n", client_path);
  else if (mkdir(client_path, 0755) == 0)
    printf("Created directory: %s\n", client_path);
  else{
    perror("Error creating directory");
    return -1;
  }
  return 1;
  
}


int savefile(int clientfd, char *path){
    // 1.Read filename length
    uint32_t fnlen;
    if (readn(clientfd, &fnlen, sizeof (fnlen)) != sizeof (fnlen)){
      perror("Failed to read filename length");
      return -1;
    }
    uint32_t filename_len = ntohl(fnlen);
    
    // 2.Read filename
    char filename[filename_len + 1];
    if (readn(clientfd, filename, filename_len) != (ssize_t)filename_len){
      perror("Failed to read filename");
      return -1;
    }
    filename[filename_len] = '\0';

    // 3. Read file size
    uint32_t fsize;
    if (readn(clientfd, &fsize, sizeof (fsize)) != sizeof (fsize)){
      perror("Failed to read file size");
      return -1;
    }
    uint32_t file_size = ntohl(fsize);

    // 4. Construct full path
    char fullpath[512];
    snprintf(fullpath, sizeof fullpath, "%s/%s", path, filename);


    FILE *fptr = fopen(fullpath, "wb");
    if (!fptr){
      perror("Failed to open file for writing.");
      return -1;
    }

    size_t received = 0;
    char buf[1024];
    while (received < file_size){
      size_t remaining = file_size - received;
      size_t to_read   = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
      ssize_t r = readn(clientfd, buf, to_read);

      if (r <= 0){
        perror("Failed to read file data");
        return -1;
      }
      fwrite(buf, 1, r, fptr);
      received += r;
    }
    fclose(fptr);
    printf("Received file: %s (%u bytes)\n", filename, file_size);

    return 1;
}

int saveDir(int clientfd, char *client_path){
  while (1){

    uint8_t flag;
    if (readn(clientfd, &flag, 1) != 1){
      perror("Failed to read flag.");
      return -1;
    }

    if(flag == 0){
      printf("Client finished sending files for %s\n",client_path);
      return 1;
    }
    
    else if (flag == 1){
      int check=savefile(clientfd, client_path);
      if(check == -1)
        return -1;
    }

    else if (flag == 2){
      char subpath[512];
      if (createDir(clientfd, client_path, subpath, sizeof (subpath)) == -1)
        return -1; 
      if (saveDir(clientfd, subpath) == -1)
        return -1;
    }
    else{
      fprintf(stderr, "Unknown flag %u\n", flag);
      return -1;
    }

  }
  
}

int main(int argc, char **argv){
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
    if (stat(DIR_NAME, &st) == 0)
    {
        printf("Directory already exists: %s\n", DIR_NAME);
    }
    else if (mkdir(DIR_NAME, 0755) == 0)
    {
        printf("Empty directory created successfully: %s\n", DIR_NAME);
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
    if(createDir(clientfd, BASE_PATH, topath, sizeof (topath)) == -1){
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