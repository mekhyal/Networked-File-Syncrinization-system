#include "client.h"

void fileToSocket(int sock, const char *path) {
  struct stat st;
  if (stat(path, &st) < 0) { 
      perror("Error stat file\n"); 
      exit(1);
  }

  // send a flag to mark this as a regular file, no need to convert it to network order using (htonl)
  // because it's just one byte
  uint8_t flag = 1;
  if (sendn(sock, &flag, sizeof(flag)) != sizeof(flag)) {
      perror("Failed to send file flag");
      exit(1);
  }

  // extract the name without (/)
  const char *filename = strrchr(path, '/');
  filename = filename ? filename + 1 : path;

  // extract the filename length 
  uint32_t fnl = strlen(filename);
  // Convert the length to network byte order
  uint32_t fnl_net = htonl(fnl);

  if (sendn(sock, &fnl_net, sizeof(fnl_net)) != sizeof(fnl_net)) {
      perror("Failed to send filename length");
      exit(1);
  }

  if (sendn(sock, filename, fnl) != fnl) {
      perror("Failed to send filename");
      exit(1);
  }

  // send the file size converted into big endian order
  uint32_t filesize = htonl(st.st_size);
  if (sendn(sock, &filesize, sizeof(filesize)) != sizeof(filesize)) {
       perror("Failed to send file size");
      exit(1);
  }

  // send the content of the file
  uint32_t fsize = st.st_size;
  checkSums(sock, path, fsize);
}