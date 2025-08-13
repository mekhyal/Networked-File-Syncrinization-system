#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zlib.h>


#define IP   "127.0.0.1"
#define PORT 4590


// Function declarations

ssize_t sendn(int fd, const void *buf, size_t n);
ssize_t readn(int fd, void *buf, size_t n);

int open_clientfd();
void fill_addr(struct sockaddr_in *a);

void fileToSocket(int sock, const char *path);

void dirToSocket(int sockFd, const char *name);

void traversDir(int sock, const char *dir);

void checkSums(int sockFd, const char *path, const uint32_t fsize);
uint32_t CRC(const char *data, uint32_t length);

#endif 
