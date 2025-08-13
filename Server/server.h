#ifndef SERVER_H
#define SERVER_H

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

#define DIR_NAME "ServerDir"
#define BASE_PATH "/home/aziz32/223Project/Files/" DIR_NAME
#define PORT 4590
#define CRC_BUF_SIZE (64 * 1024)

ssize_t readn(int, void *, size_t);
ssize_t sendn(int, void *, size_t);

int open_listenfd(int);
int createDir(int, char *, char *, int);
uint32_t calculate_crc(FILE *f, size_t bytes_to_crc);
int checksum(char *, uint32_t, uint32_t, uint32_t);
int savefile(int, char *);
int saveDir(int, char *);

#endif
