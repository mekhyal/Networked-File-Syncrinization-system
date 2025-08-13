#include "client.h"

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n) {
  size_t  left = n;
  ssize_t nread;
  char   *p = buf;

  while (left > 0) {
    if ((nread = read(fd, p, left)) < 0){
        if (errno == EINTR) 
          nread = 0;
        else                
          return -1;
    }
    else if (nread == 0) {
        break;   // EOF
    }
    left -= nread;
    p    += nread;
  }
  return n - left;  // bytes actually read
}



ssize_t sendn(int fd, const void *buf, size_t n) {
  size_t nleft = n;
  ssize_t nwritten;
  const char *bufp = buf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR)  /* interrupted by signal handler */
          nwritten = 0;    /* and call write() again */
      else
        return -1;       /* real error, errno set by write() */
    }
    nleft -= nwritten;
    bufp  += nwritten;
  }
  return n;              /* return number of bytes requested */
}

