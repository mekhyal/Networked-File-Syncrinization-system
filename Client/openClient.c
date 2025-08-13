#include "client.h"



void fill_addr(struct sockaddr_in *a) {
  memset(a, 0, sizeof(*a));
  a->sin_family = AF_INET;
  a->sin_port = htons(PORT);
  if (!inet_aton(IP, &a->sin_addr)) {
      perror("Invalid IP address");
      exit(1);
  }
}

int open_clientfd() {
  int sockFd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockFd < 0) {
      perror("Error creating socket");
      exit(1);
  }

  struct sockaddr_in server_addr;
  fill_addr(&server_addr);  // Fill IP + port

  if (connect(sockFd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      perror("Error connecting to server");
      close(sockFd);
      exit(1);
  }

  return sockFd;
}
