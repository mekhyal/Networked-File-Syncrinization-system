#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdint.h>

#define dir_name "ServerDir"
#define PORT 4590


int open_listenfd(int port){

  int serverfd;
  struct sockaddr_in server_addr;

  //Create socket for the server and return to variable serverfd 
  if((serverfd=socket(AF_INET , SOCK_STREAM, 0)) < 0){
    perror("Socket Failed..");
    exit(1);
  }

  /*First Fill the server_addr to avoid garbage data for safety
  /Specify IPv4 addressing
  Accept any IP address
  PORT is your server port number.*/
  memset(&server_addr,0,sizeof(server_addr));
  server_addr.sin_family=AF_INET;
  server_addr.sin_addr.s_addr=INADDR_ANY;
  server_addr.sin_port=htons(port);

  //bind() is the function that tells the OS:
  //“Hey, I want to listen for connections on this IP and this port.”
  if((bind(serverfd,(struct sockaddr *)&server_addr,sizeof(server_addr))) < 0){
    perror("There is error of using bind...\n");
    close(serverfd);
    exit(1);
  }

  //Listen for incoming connections
  if (listen(serverfd, 5) < 0){
    perror("Listen failed.");
    close(serverfd);
    exit(1);
  }


  return serverfd;

}


//if Directory exist does not create if not will be create for client
//stat helps you to check the file or directory exist or not 
//Need more optimization
void createDir(int client,char *finalpath,int s){
  struct stat statbuf;
  char curr[200]="/home/aziz32/223Project/Files/ServerDir/";
  
  // Received from client (by socket client) the length of the directory name
  int len;
  if (read(client, &len, sizeof(len)) <= 0) {
      perror("Failed to read length");
      return;
  }

  char dirName[len + 1];  // +1 for the null terminator

  // Received from client (by socket client) the directory name
  if (read(client, dirName, len) <= 0) {
      perror("Failed to read directory name");
      return;
  }

  //C string‐functions (like printf("%s"), snprintf(), strcpy(), etc.) all look 
  //for a “zero” ('\0') byte to know where the string ends.dirName[len] = '\0';
  strncat(curr,dirName,sizeof(curr) - strlen(curr) -1);
  snprintf(finalpath, s, "%s", curr);
  
  //The return value is 0 if the operation is successful(exist), or -1 on failure
  // Check if directory exists or create it
  if (stat(curr, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
      printf("Directory already exists: %s\n", curr);
  } else {
      if (mkdir(curr, 0755) == 0) {
          printf("Directory created: %s\n", curr);
      } else {
          perror("Error creating directory");
      }
  }


}


void savefile(int client,char *path){
  //Use uint32_t to make sure you use 4-bytes regardless for all platform

  /*steps:
  1.read file length (use ntoh(lors))
  2.read file name(with exaclty size) & add /0 to the end of filename
  3.read file size (use ntoh(lors))
  4.open file (create the file)
  5.Receive and write file content
  6.close file
  */

  char buf[1024];
  uint32_t  filename_len,filesize,received=0;
  //struct stat check;

  //Received from client (by socket client) the lenght of file name
  if(read(client,&filename_len,sizeof(uint32_t)) <=0){
    perror("Failed to read filename length");
    return;
  }
  //convert byte order from big Endian to little Endian
  filename_len=ntohl(filename_len);
  char filename[filename_len+1];

  //Received from client (by socket client) the filename
  if(read(client,filename,filename_len) <=0){
    perror("Failed to read filename");
    return;
  }
  
  filename[filename_len]='\0';


  //Received from client (by socket client) the file size
  if(read(client,&filesize,sizeof(uint32_t)) <=0){
    perror("Failed to read filename length");
    return;
  }
  
  //convert byte order from big Endian to little Endian
  filesize=ntohl(filesize);

  //construct full path  
  char fullpath[500];
  snprintf(fullpath,sizeof(fullpath),"%s/%s",path,filename);


  /*Always use "rb" when reading files to send over sockets it's sending any file reliably
  The difference between recv and read:
  read: works on any file descriptor, including sockets, pipes, regular files.
  recv: specifically designed for sockets, and lets you pass additional flags.
  */
 
  FILE *fptr=fopen(fullpath,"wb");
  if(!fptr){
    perror("Failed to open file for writing.");
    return;
  }

  while(received < filesize){
    uint32_t bytes=read(client,buf,sizeof(buf));
    //if there is any error with reading or connection
    if(bytes <= 0 ){
      printf("There is error from reading from file.\n");
      fclose(fptr);
      break;
    }
    fwrite(buf,1,bytes,fptr);
    received+=bytes;
  }

  fclose(fptr);
  printf("Received file: %s (%u bytes)\n",filename,filesize);
  
}


// ./client Base client 
//client/ Base

int main(int argc , char **argv){
  int clientfd,serverfd,port;
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  struct stat statbuf;

  if(argc != 2){
    printf("Usage: ./%s <port>\n",argv[0]);
    exit(EXIT_FAILURE);
  }

  port=atoi(argv[1]);
  serverfd=open_listenfd(port);

  if(serverfd < 0){
    perror("Server setup failed.\n");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d...\n", port);

  //The return value is 0 if the operation is successful(exist), or -1 on failure
  if (stat(dir_name, &statbuf) == 0) {
    printf("Directory already exists.\n");
  }
  else {
    if (mkdir(dir_name, 0755) == 0) {
      printf("Empty directory created successfully.\n");
    }
    else {
      perror("Error creating directory");
      exit(EXIT_FAILURE);
    }
  }

  // always waiting for new clients to connect for now sequential
  //while(1){
    if((clientfd = accept(serverfd ,(struct sockaddr *)&client_addr,&addr_len)) < 0){
      perror("Accept Failed");
      //continue;
      exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");
    char path[256];
    createDir(clientfd,path,256);
    printf("current path : %s\n",path);
    //savefile(clientfd,path);

    
    while(1){
      uint8_t flag;
      if(read(clientfd,&flag,sizeof(uint8_t)) <=0){
        perror("Failed to read done flag.");
        break;
      }
        
      if (flag==0)
      {
        printf("Client finished sending files.\n");
        break;
      }
      savefile(clientfd,path);
    }

    close(clientfd);
  //}

  
  close(serverfd);
  return 0;
}