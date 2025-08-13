#include "server.h"

//if Directory exist does not create if not will be create for client
//stat helps you to check the file or directory exist or not 
int createDir(int clientfd,char *server_path, char *client_path , int client_path_len)
{
  // Received from client (by socket client) the length of the directory name
  uint32_t len;
  if (readn(clientfd, &len, sizeof (len)) != sizeof (len)){
    perror("Failed to read length");
    return -1;
  }
  len = ntohl(len);

  if (len == 0 || len > 255) {
    fprintf(stderr, "Invalid directory name length\n");
    return -1;
  }

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