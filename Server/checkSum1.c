#include "server.h"


uint32_t calculate_crc(FILE *f, size_t bytes_to_crc) {
  uint8_t buf[CRC_BUF_SIZE];                  // small, fixed buffer
  uint32_t crc = crc32(0L, Z_NULL, 0);        // initialize
  size_t left = bytes_to_crc;                 // how many bytes we need

  while (left){
    size_t to_read = (left < CRC_BUF_SIZE)? left : CRC_BUF_SIZE;   // cap at buffer size
    size_t got = fread(buf, 1, to_read, f);
    
    if (got != to_read){
      perror("Failed to calculate the CRC");                // I/O error or unexpected EOF
      return 0;
    }

    crc = crc32(crc, buf, got);               // update running CRC
    left -= got;                              // decrease bytes remaining
  }

  return crc;                                 // final checksum
}


int checksum(char *path, uint32_t fsize ,uint32_t exp_crc1, uint32_t exp_crc2){
  
  FILE *fptr = fopen(path,"rb");
  if (!fptr){ 
    perror("ERROR: OPEN FILE"); 
    return -1; 
  }

  uint32_t half1 = fsize/2;
  uint32_t half2 = fsize - half1;

  // first half
  uint32_t crc1 = calculate_crc(fptr, half1);

  // Seek to second half
  fseek(fptr, half1, SEEK_SET);

  // second half
  uint32_t crc2 = calculate_crc(fptr, half2);

  fclose(fptr);

  if(crc1 == 0 || crc2 == 0){
    perror("ERROR: of calculate the crc");
    return -1 ; 
  }

  if (crc1 != exp_crc1) 
    return 4;
  if (crc2 != exp_crc2) 
    return 5;

  return 6; // there is no changes 
}