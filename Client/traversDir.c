#include "client.h"

void traversDir(int sock, const char *dir) {
  DIR *d = opendir(dir);
  if (!d) {
      perror("Error opening dir\n");
      exit(1);
  }

  struct dirent *e;

  // extract (only the files) from the client dir:
  while ((e = readdir(d)) != NULL) {
      // GPT GENERATED LINES : 120-121
      // to avoid infinite loops, we may enter the ((.)the current dir) or ((..)the parent dir), which don't contain the actual files
      if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
          continue;

      if (e->d_type == DT_REG) {
          // extract the path
          char path[1024];
          snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
          fileToSocket(sock, path);
      }
  }

  // rewind the dir to retraverse it looking for subdirs.
  rewinddir(d);

  // extract only the dirs:
  while ((e = readdir(d)) != NULL) {
      // GPT GENERATED LINES : 138-139
      // to avoid infinite loops, we may enter the ((.)the current dir) or ((..)the parent dir), which don't contain the actual files
      if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
          continue;

      if (e->d_type == DT_DIR) {
          // extract the path
          char p[1024];
          snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);

          uint8_t dflag = 2;  // send a flag to tell the server there is subdir incoming..
          if (sendn(sock, &dflag, sizeof(dflag)) != sizeof(dflag)) {
              perror("Failed to send subdir flag");
              exit(1);
          }

          dirToSocket(sock, e->d_name); // send the info of the dir (len of the name and name)
          traversDir(sock, p); // recursive call to traverse its files or sub dirs.
      }
  }

  closedir(d);

  // send a flag to tell the server this is the end of the local dir
  uint8_t end = 0;
  if (sendn(sock, &end, sizeof(end)) != sizeof(end)) {
      perror("Failed to send end flag for directory");
      exit(1);
  }
}