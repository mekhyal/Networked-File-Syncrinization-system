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
#include <signal.h>
#include <sys/wait.h>


#define DIR_NAME "ServerDir"
#define BASE_PATH "/home/aziz32/223Project/Files/" DIR_NAME
#define PORT 4590


void sig_handler(int sig){
  while(waitpid(-1,0,WNOHANG) > 0);
  return;
}

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

ssize_t sendn(int fd, void *buf, size_t n) {
  size_t sent = 0;
  char *p = buf;
  while (sent < n) {
      ssize_t r = send(fd, p + sent, n - sent, 0);
      if (r < 0) {
          if (errno == EINTR) continue;
          perror("[Server] some error on sendn function");
          exit(1);
      }
      sent += r;
  }
  return sent;
}

int traverse_dir(int clinetfd,char *client_path) {
    DIR *dir = opendir(client_path);
    if (!dir) {
      perror("[Server] Failed to open directory");
      return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        

        uint32_t len=strlen(entry->d_name);
        uint32_t len_c=htonl(len);
        if(sendn(clinetfd,&len_c,4) != 4){
          perror("[Server] Failed to Send length of file name");
          return -1;
        }
        printf("[Server] Sending the file length to Client: %u\n",(unsigned)strlen(entry->d_name));

        if(sendn(clinetfd,entry->d_name,len) != len){
          perror("[Server] Failed to Send file name");
          return -1;
        }
        printf("[Server] Send the file to Client: %s\n",entry->d_name);
    }
    uint32_t len=0;
    if(sendn(clinetfd,&len,4) != 4){
      perror("[Server] Failed to Send flag to stop");
      return -1;
    }
    
    printf("[Server] Finishing to sends all files name\n");

    closedir(dir);
    return 0;
}

int sendFileClient(int clientfd,char *client_path){

  int n=256;
  char fullpath[n];
  uint8_t flag;
  while(1){
    if(readn(clientfd,&flag,1) != 1){
      perror("[Server] Failed To Received flag");
      return -1;
    }

    if(flag == 9){
      uint32_t flen;
      if(readn(clientfd ,&flen,4 ) != 4){
        perror("[Server] Failed Received file name length");
        return -1;
      }
      flen=ntohl(flen);
      printf("[Server] Received from client file name lenght: %u\n",flen);

      char filename[flen +1];
      if(readn(clientfd ,filename,flen ) != flen){
        perror("[Server] Failed Received file name");
        return -1;
      }
      filename[flen]='\0';
      printf("[Server] Received from client file name: %s\n",filename);

      snprintf(fullpath, n, "%s/%s", client_path, filename);
      printf("[Server] Full path for file: %s\n",fullpath);

      FILE *fptr = fopen(fullpath, "rb");
      if (!fptr) {
          perror("[Server] Failed to open file for Reading.");
          return -1;
      }

      //get file size
      fseek(fptr, 0, SEEK_END);
      uint32_t file_size = ftell(fptr);
      fseek(fptr, 0, SEEK_SET);

      // Send file size
      uint32_t net_file_size = htonl(file_size);
      if (sendn(clientfd, &net_file_size, sizeof(net_file_size)) != sizeof(net_file_size)) {
          perror("[Server] Failed to send file size");
          fclose(fptr);
          return -1;
      }
      printf("[Server] Sending to client file size: %u\n",file_size);

      // Send file content
      char buffer[1024];
      size_t bytes_sent = 0;
      while (bytes_sent < file_size) {
          size_t to_read = (file_size - bytes_sent) < sizeof(buffer) ? (file_size - bytes_sent) : sizeof(buffer);
          size_t read_bytes = fread(buffer, 1, to_read, fptr);
          if (read_bytes <= 0) {
              perror("[Server] Failed to read from file");
              fclose(fptr);
              return -1;
          }

          if (sendn(clientfd, buffer, read_bytes) != read_bytes) {
              perror("[Server] Failed to send file data");
              fclose(fptr);
              return -1;
          }

          bytes_sent += read_bytes;
      }

      fclose(fptr);
      printf("[Server] Sent file: %s (%u bytes)\n", filename, file_size);
    }

    else if(flag == 0){
      printf("[Server] Done Download Files from Server\n");
      return 0;
    }
    else{
      perror("[Server] Unknown Flag Received");
      return -1;
    }
      
  }

}

int createDir(int clientfd,char *server_path, char *client_path , int client_path_len){
  // Received from client (by socket client) the length of the directory name
  uint32_t len;
  if (readn(clientfd, &len, sizeof (len)) != sizeof (len)){
    perror("Failed to read length");
    return -1;
  }
  len = ntohl(len);
  printf("[Server] Received from client Directory length: %u\n",len);
  

  // Received from client (by socket client) the directory name
  char name[len + 1]; // +1 for the null terminator
  if (readn(clientfd, name, len) != (ssize_t)len){
    perror("Failed to read directory name");
    return -1;
  }
  name[len] = '\0';
  printf("[Server] Received from client Directory Name: %s\n",name);

  // build full path under parent_path
  snprintf(client_path, client_path_len, "%s/%s", server_path, name);

  //The return value is 0 if the operation is successful(exist), or -1 on failure
  // Check if directory exists or create it
  struct stat st;
  if (stat(client_path, &st) == 0 && S_ISDIR(st.st_mode))
    printf("[Server] Directory already exists: %s\n", client_path);
  else if (mkdir(client_path, 0755) == 0)
    printf("[Server] Created directory: %s\n", client_path);
  else{
    perror("Error creating directory");
    return -1;
  }
  return 1;
  
}

void fileUpdate(char *filename, uint8_t result1, uint8_t result2, int clientfd, uint32_t fsize) {
  uint8_t update_flag = 0;

  if (result1 == 5 && result2 == 6) update_flag = 7;  // both
  else if (result1 == 5) update_flag = 5;             // first only
  else if (result2 == 6) update_flag = 6;             // second only
  else update_flag = 0;  // no update needed

  sendn(clientfd, &update_flag, 1);
  if (update_flag == 0) {
      printf("[Server] No update needed.\n");
      return;
  }

  FILE *fp = fopen(filename, "r+b");
  if (!fp) {
      perror("[Server] fopen failed in fileUpdate");
      return;
  }

  uint32_t first_half = fsize / 2;
  uint32_t second_half = fsize - first_half;

  if (update_flag == 5 || update_flag == 7) {
      char *buf1 = malloc(first_half);
      if (readn(clientfd, buf1, first_half) != (ssize_t)first_half) {
          perror("[Server] Failed to receive first chunk");
          free(buf1);
          fclose(fp);
          return;
      }
      fseek(fp, 0, SEEK_SET);
      fwrite(buf1, 1, first_half, fp);
      printf("[Server] Updated first chunk\n");
      free(buf1);
  }

  if (update_flag == 6 || update_flag == 7) {
      char *buf2 = malloc(second_half);
      if (readn(clientfd, buf2, second_half) != (ssize_t)second_half) {
          perror("[Server] Failed to receive second chunk");
          free(buf2);
          fclose(fp);
          return;
      }
      fseek(fp, first_half, SEEK_SET);
      fwrite(buf2, 1, second_half, fp);
      printf("[Server] Updated second chunk\n");
      free(buf2);
  }

  fclose(fp);
}

int splitFile(char *filename, uint32_t exp_crc1, uint32_t exp_crc2, uint32_t client_fsize,int clientfd) {
  uint32_t first_half = client_fsize / 2;
  uint32_t second_half = client_fsize - first_half;

  FILE *fptr = fopen(filename, "rb");
  if (!fptr) {
      perror("File open failed");
      return -1;
  }

  // Get server file size
  fseek(fptr, 0L, SEEK_END);
  uint32_t server_fsize = ftell(fptr);
  rewind(fptr);

  printf("[Server] Server file size: %u, Client file size: %u\n", server_fsize, client_fsize);

  // Allocate buffers
  char *buf1 = calloc(1, first_half);
  char *buf2 = calloc(1, second_half);

  if (!buf1 || !buf2) {
      perror("Memory allocation failed");
      fclose(fptr);
      return -1;
  }

  // Read up to the size available
  fread(buf1, 1, (server_fsize >= first_half) ? first_half : server_fsize, fptr);
  if(server_fsize > first_half){
      fread(buf2, 1, server_fsize - first_half, fptr);
  }

  fclose(fptr);

  // Compute CRCs
  uint32_t crc1 = crc32(0L, Z_NULL, 0);
  crc1 = crc32(crc1, (const Bytef *)buf1, first_half);

  uint32_t crc2 = crc32(0L, Z_NULL, 0);
  crc2 = crc32(crc2, (const Bytef *)buf2, second_half);

  printf("[Server] Calculated CRC1: %u\n", crc1);
  printf("[Server] Calculated CRC2: %u\n", crc2);

  uint8_t result1,result2;
  // Compare CRCs
  if (crc1 != exp_crc1) {
      printf("[Server] First half needs update.\n");
      result1=5;
  }else{
    printf("[Server] First half Does Not needs update.\n");
    result1=0;
  }
  if (crc2 != exp_crc2) {
      printf("[Server] Second half needs update.\n");
      result2=6;
  }else{
    printf("[Server] Second half Does Not needs update.\n");
    result2=0;
  }

  free(buf1);
  free(buf2);

  fileUpdate(filename, result1, result2, clientfd, client_fsize);

  return 0;
}

int checkfile(char *filepath) {
  struct stat sa;
  if (stat(filepath, &sa) == 0) {
    printf("[Server] File exists: %s\n", filepath);
    return 1; 
  }else {
    printf("[Server] File does NOT exist: %s\n", filepath);
    return 0;  
  }

}

int savefile(int clientfd, char *path){
    // 1.Read filename length
    uint32_t fnlen;
    if (readn(clientfd, &fnlen, sizeof (fnlen)) != sizeof (fnlen)){
      perror("Failed to read filename length");
      return -1;
    }
    uint32_t filename_len = ntohl(fnlen);
    printf("[Server] Received from client file length: %u\n",filename_len);
    
    // 2.Read filename
    char filename[filename_len + 1];
    if (readn(clientfd, filename, filename_len) != (ssize_t)filename_len){
      perror("Failed to read filename");
      return -1;
    }
    filename[filename_len] = '\0';

    printf("[Server] Received from client file name: %s\n",filename);

    printf("[Server] Check if file exist or not \n");

   
    // 3. Construct full path
    char fullpath[512];
    snprintf(fullpath, sizeof fullpath, "%s/%s", path, filename);
    printf("[Server] Construct full path for new file: %s\n",fullpath);

    int check1=checkfile(fullpath);
    //if the file exist will return 1 else 0
    //so if equal 1 means file exist
    if(check1== 1){
      uint8_t flag=3;
      sendn(clientfd,&flag,1);
      printf("[Server] Sending to client flag: %u\n",flag);

      uint32_t fsize;
      if (readn(clientfd, &fsize, sizeof (fsize)) != sizeof (fsize)){
        perror("Failed to read file size");
        return -1;
      }
      uint32_t file_size = ntohl(fsize);
      printf("[Server] Received from client file Size: %u\n",file_size);
      
      uint32_t c,c1;
      if(readn(clientfd,&c , 4) != 4){
        perror("Failed To Received CRC 1 from Client\n");
        return -1;
      }
      uint32_t crc1=ntohl(c);
      printf("[Server] Received From Client CRC1: %u\n",crc1);

      if(readn(clientfd,&c1 , 4) != 4){
        perror("Failed To Received CRC 1 from Client\n");
        return -1;
      }
      uint32_t crc2=ntohl(c1);
      printf("[Server] Received From Client CRC2: %u\n",crc2);

      splitFile(fullpath,crc1,crc2,file_size,clientfd);

      return 1;
    }
    //File does not exist Received the size and the content 
    else if(check1 == 0){
      uint8_t flag=4;
      sendn(clientfd,&flag,1);
      printf("[Server] Sending to client flag: %u\n",flag);
      // 4. Read file size
      uint32_t fsize;
      if (readn(clientfd, &fsize, sizeof (fsize)) != sizeof (fsize)){
        perror("Failed to read file size");
        return -1;
      }
      uint32_t file_size = ntohl(fsize);
      printf("[Server] Received from client file Size: %u\n",file_size);


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
      printf("[Server] Received file: %s (%u bytes)\n", filename, file_size);
    }
  

    return 1;
}

int saveDir(int clientfd, char *client_path){
  while (1){

    uint8_t flag;
    if (readn(clientfd, &flag, 1) != 1){
      perror("Failed to read flag.");
      return -1;
    }

    printf("[Server] Received from client flag: %u\n",flag);

    if(flag == 0){
      printf("[Server] Client finished sending files for %s\n",client_path);
      return 1;
    }
    
    else if (flag == 1){
      int check=savefile(clientfd, client_path);
      if(check == -1)
        return -1;
      printf("[Server] finished saving file on %s\n",client_path);  
    }

    else if (flag == 2){
      char subpath[512];
      if (createDir(clientfd, client_path, subpath, sizeof (subpath)) == -1)
        return -1;  
      if (saveDir(clientfd, subpath) == -1)
        return -1;
    }

    else if(flag == 8){
      if(traverse_dir(clientfd,client_path) == -1){
        perror("[Server] Error to traverse_directory");
        return -1;
      }
      sendFileClient(clientfd,client_path); 
    }
    else{
      fprintf(stderr, "Unknown flag %u\n", flag);
      return -1;
    }

  }
  
}

int main(int argc, char **argv){

    int serverfd,clientfd;
    struct stat st;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof client_addr;

    signal(SIGCHLD,sig_handler);


    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    serverfd = open_listenfd(atoi(argv[1]));
    if (serverfd < 0)
    {
        perror("Server setup failed.\n");
        return 1;
    }

    printf("[Server] listening on port %d...\n", atoi(argv[1]));

    
    if (stat(DIR_NAME, &st) == 0)
    {
        printf("[Server] Directory already exists: %s\n", DIR_NAME);
    }
    else if (mkdir(DIR_NAME, 0755) == 0)
    {
        printf("[Server] Empty directory created successfully: %s\n", DIR_NAME);
    }
    else
    {
        perror("Error creating directory");
        return 1;
    }

    while(1){
      clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &addrlen);
      if (clientfd < 0){
        perror("Accept Failed");
        continue;
      }

      if(fork()==0){
        close(serverfd);
        printf("[Server] Client Connected To Server.\n");
        printf("[Server] Sleeping for 15 sec for Client connection\n");
        sleep(15);

      // read and create the top-level directory under BASE_PATH
      char topath[512];
      if(createDir(clientfd, BASE_PATH, topath, sizeof (topath)) == -1){
        perror("Failed to create directory");
        close(clientfd);
        exit(1);
      }
      printf("[Server] Syncing under Path: %s\n", topath);

      // handle all files & subdirectories recursively
      if(saveDir(clientfd, topath) == -1){
        perror("Failed to create directory");
        close(clientfd);
        exit(1);
      }
      close(clientfd);
      exit(0);

    }
    close(clientfd);
  }    
    
  return 0;
}