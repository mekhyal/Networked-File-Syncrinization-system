// client.c
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

#define IP   "127.0.0.1"
#define PORT 4590

void fill_addr(struct sockaddr_in *a) {
    memset(a,0,sizeof *a);
    a->sin_family = AF_INET;
    a->sin_port   = htons(PORT);
    if (!inet_aton(IP,&a->sin_addr)) {
        fprintf(stderr,"invalid IP\n"); exit(1);
    }
}

ssize_t sendn(int fd, const void *buf, size_t n) {
    size_t sent=0;
    const char *p=buf;
    while(sent<n) {
        ssize_t r=send(fd,p+sent,n-sent,0);
        if (r<0) {
            if(errno==EINTR) continue;
            perror("send"); exit(1);
        }
        sent+=r;
    }
    return sent;
}

void send_dirname(int sock, const char *name) {
    uint32_t len = (uint32_t)strlen(name);
    uint32_t nl  = htonl(len);
    sendn(sock,&nl,sizeof nl);
    sendn(sock,name,len);
}

void send_file(int sock, const char *path) {
    struct stat st;
    if(stat(path,&st)<0){perror("stat");exit(1);}

    uint8_t flag=1;
    sendn(sock,&flag,sizeof flag);

    const char *bn = strrchr(path,'/');
    bn = bn?bn+1:path;
    uint32_t fnl = htonl((uint32_t)strlen(bn));
    sendn(sock,&fnl,sizeof fnl);
    sendn(sock,bn,ntohl(fnl));

    uint32_t fsz = htonl((uint32_t)st.st_size);
    sendn(sock,&fsz,sizeof fsz);

    FILE *f=fopen(path,"rb");
    if(!f){perror("fopen");exit(1);}
    char buf[4096];
    size_t r;
    while((r=fread(buf,1,sizeof buf,f))>0)
        sendn(sock,buf,r);
    fclose(f);
}

void recurse_send(int sock, const char *folder) {
    DIR *d=opendir(folder);
    if(!d){perror("opendir");exit(1);}
    struct dirent *e;

    // 1) files
    while((e=readdir(d))!=NULL){
        if(e->d_name[0]=='.'&&
          (e->d_name[1]=='\0'||
          (e->d_name[1]=='.'&&e->d_name[2]=='\0')))
            continue;
        if(e->d_type==DT_REG){
            char p[1024];
            snprintf(p,sizeof p,"%s/%s",folder,e->d_name);
            send_file(sock,p);
        }
    }
    rewinddir(d);

    // 2) subdirs
    while((e=readdir(d))!=NULL){
        if(e->d_name[0]=='.'&&
          (e->d_name[1]=='\0'||
          (e->d_name[1]=='.'&&e->d_name[2]=='\0')))
            continue;
        if(e->d_type==DT_DIR){
            char p[1024];
            snprintf(p,sizeof p,"%s/%s",folder,e->d_name);

            uint8_t dflag=2;               // <-- NEW: directory flag
            sendn(sock,&dflag,sizeof dflag);

            send_dirname(sock,e->d_name); // <-- send only basename
            recurse_send(sock,p);
        }
    }
    closedir(d);

    uint8_t end=0;
    sendn(sock,&end,sizeof end);
}

int main(int argc, char **argv){
    if(argc<2){
        fprintf(stderr,"Usage: %s <local_dir> [remote_name]\n",argv[0]);
        exit(1);
    }
    const char *local = argv[1];
    const char *remote = (argc>2?argv[2]:strrchr(local,'/')?strrchr(local,'/')+1:local);

    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){perror("socket");exit(1);}
    struct sockaddr_in srv;
    fill_addr(&srv);
    if(connect(sock,(struct sockaddr*)&srv,sizeof srv)<0){
        perror("connect");exit(1);
    }

    // initial dir (no flag)
    send_dirname(sock,remote);

    recurse_send(sock,local);

    close(sock);
    return 0;
}
