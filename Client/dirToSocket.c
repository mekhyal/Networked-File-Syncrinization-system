#include "client.h"

void dirToSocket(int sockFd, const char *name) {
  uint32_t len = strlen(name);
  uint32_t nl = htonl(len); // the conversion due to network order in the socket
  if (sendn(sockFd, &nl, sizeof(nl)) != sizeof(nl)) {
      perror("Failed to send directory name length");
      exit(1);
  }

  if (sendn(sockFd, name, len) != len) {
      perror("Failed to send directory name");
      exit(1);
  }
}