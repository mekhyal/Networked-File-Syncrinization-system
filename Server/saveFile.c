#include "server.h"

// savefile: handle CRC negotiation then receive changed data
int savefile(int clientfd, char *path)
{
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

    struct stat st;
    if (stat(fullpath, &st) < 0){
      printf("File does not exist → send whole content.\n");
      uint8_t flag = 3;
      if(sendn(clientfd, &flag, 1) != 1){
        perror("Failed to send flag to client.");
        return -1;
      }
    }else{
        printf("File exists → send expected CRCs.\n");
        uint32_t crc1, crc2;
        uint8_t flag = 7;
        if(sendn(clientfd, &flag, 1) != 1){
          perror("Failed to send flag to client.");
          return -1;
        }

        if (readn(clientfd, &crc1, sizeof (crc1)) != sizeof (crc1) ||
            readn(clientfd, &crc2, sizeof (crc2)) != sizeof (crc2))
        {
          perror("Failed to read CRCs from Client");
          return -1;
        }
        uint32_t expected_crc1 = ntohl(crc1);
        uint32_t expected_crc2 = ntohl(crc2);
        
        int result_flag = checksum(fullpath, file_size,expected_crc1, expected_crc2);
        if(result_flag == -1){
          perror("ERROR: CheckSum function");
          return -1;
        }
        uint8_t result_f=result_flag;

        if(sendn(clientfd, &result_f, sizeof (result_f)) != 1){
          perror("ERROR TO SEND FLAG");
          return -1;
        }
        
        if(result_flag == 6){
            printf("There is no changes.\n");
            return 1;
        }

        // For changes, client will resend new file_size + data
        if (readn(clientfd, &fsize, sizeof (fsize)) != sizeof (fsize)){
          perror("Failed to read new file size");
          return -1;
        }
        file_size = ntohl(fsize);
    }

    // 6. Receive and write file content either full content or chunk
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