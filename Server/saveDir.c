#include "server.h"

int saveDir(int clientfd, char *client_path){
  while (1){

    uint8_t flag;
    if (readn(clientfd, &flag, 1) != 1){
        perror("Failed to read flag.");
        return -1;
    }

    if(flag == 0){
      printf("Client finished sending files for %s.\n",client_path);
      return 1;
    }else if (flag == 1){
      // regular file
      int check=savefile(clientfd, client_path);
      if(check == -1){
        return -1;
      }

    }else if (flag == 2){
      char subpath[512];
      // reuse createDir to read name and mkdir
      if (createDir(clientfd, client_path, subpath, sizeof (subpath)) == -1)
        return -1;
      // Recursively save contents of the new subdirectory  
      if (saveDir(clientfd, subpath) == -1)
        return -1;
    }else{
      fprintf(stderr, "Unknown flag %u\n", flag);
      return -1;
    }

  }
  
}