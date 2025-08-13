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
#include <libgen.h>     
#include <signal.h>

#define files_size 100
#define length_files_size 256
#define MAX_PATH_LEN 512

#define IP   "127.0.0.1"
#define PORT 4590


//signal handler (CTRL + C & CTRL + Z)
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n[Client] Caught (Ctrl+C) Continuing program...\n");
    }
    else if (sig == SIGTSTP) {
        printf("\n[Clinet] Caught (Ctrl+Z) Continuing program instead of suspending...\n");
    }
    fflush(stdout);
}

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
            perror("send");exit(1); 
            //return -1;
        }
        sent+=r;
    }
    return sent;
}

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n){
  size_t left = n;
  char *p = buf;
  while (left > 0)
  {
    ssize_t r = read(fd, p, left);
    if (r < 0)
    {
        if (errno == EINTR)
            continue;
        return -1;
    }
    else if (r == 0)
    {
        return 0;
    }
    left -= r;
    p += r;
  }
  return n;
}

//send to server directory 
void send_dirname(int sock, const char *name) {
    uint32_t len = (uint32_t)strlen(name);
    uint32_t nl  = htonl(len);

    if (sendn(sock, &nl, sizeof nl) != sizeof nl) {
        perror("Failed to send directory name length");
        exit(1);
    }

    if (sendn(sock, name, len) != len) {
        perror("Failed to send directory name");
        exit(1);
    } 
} 

//send to server the file 
void send_file(int sock, const char *path) {
    // grap the metadata of the file, to use the file size later for chunks purpose
    struct stat st;
    if (stat(path, &st) < 0) {
        perror("stat");
        exit(1);
    }

    uint8_t flag = 1;
    if (sendn(sock, &flag, sizeof flag) != sizeof flag) {
        perror("Failed to send file flag");
        exit(1);
    }

    // AI generated, to extract only the file name without the /
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;

    uint32_t fnl = htonl((uint32_t)strlen(fname));
    if (sendn(sock, &fnl, sizeof fnl) != sizeof fnl) {
        perror("Failed to send filename length");
        exit(1);
    }

    if (sendn(sock, fname, strlen(fname)) != strlen(fname)) {
        perror("Failed to send filename");
        exit(1);
    }

    uint8_t check;
    if (readn(sock, &check, sizeof(check)) != sizeof(check)) {
        perror("Failed to read server check byte");
        exit(1);
    }
    //printf("[Client] Received From Server flag: %u\n", check);

    if (check == 3){
        // Server says file exists, send CRCs
        printf("[Client] File exists. Sending CRCs...\n");

        uint32_t fsize = (uint32_t)st.st_size;
        uint32_t first_half = fsize / 2;
        uint32_t second_half = fsize - first_half;

        FILE *fp = fopen(path, "rb");
        if (!fp) {
            perror("fopen");
            exit(1);
        }

        char *buf1 = malloc(first_half);
        char *buf2 = malloc(second_half);

        if (!buf1 || !buf2) {
            fprintf(stderr, "malloc failed for buf1 or buf2\n");
            fclose(fp);
            exit(1);
        }

        if (fread(buf1, 1, first_half, fp) != first_half) {
            perror("fread failed for buf1");
            free(buf1); free(buf2);
            fclose(fp);
            exit(1);
        }

        if (fread(buf2, 1, second_half, fp) != second_half) {
            perror("fread failed for buf2");
            free(buf1); free(buf2);
            fclose(fp);
            exit(1);
        }

        fclose(fp);

        uint32_t crc1 = crc32(0L, Z_NULL, 0);
        crc1 = crc32(crc1, (const unsigned char *)buf1, first_half);

        uint32_t crc2 = crc32(0L, Z_NULL, 0);
        crc2 = crc32(crc2, (const unsigned char *)buf2, second_half);

        // Send file size and CRCs
        uint32_t net_fsize = htonl(fsize);
        if (sendn(sock, &net_fsize, sizeof net_fsize) != sizeof net_fsize) {
            perror("Failed to send file size");
            exit(1);
        }

        //printf("[Client] Sending To Server file size: %u\n", fsize);
        printf("[Client] Sending To Server CRC1: %u\n", crc1);
        printf("[Client] Sending To Server CRC2: %u\n", crc2);

        uint32_t net_crc1 = htonl(crc1);
        uint32_t net_crc2 = htonl(crc2);

        if (sendn(sock, &net_crc1, sizeof net_crc1) != sizeof net_crc1) {
            perror("Failed to send CRC1");
            exit(1);
        }

        if (sendn(sock, &net_crc2, sizeof net_crc2) != sizeof net_crc2) {
            perror("Failed to send CRC2");
            exit(1);
        }

        // Receive update flag from server
        uint8_t update_flag;
        if (readn(sock, &update_flag, 1) != 1) {
            perror("Failed to read update flag");
            exit(1);
        }
        //printf("[Client] Server requested update flag: %u\n", update_flag);

        if (update_flag == 5 || update_flag == 7) {
            if (sendn(sock, buf1, first_half) != first_half) {
                perror("Failed to send first chunk");
                exit(1);
            }
            printf("[Client] Sent first chunk\n");
        }

        if (update_flag == 6 || update_flag == 7) {
            if (sendn(sock, buf2, second_half) != second_half) {
                perror("Failed to send second chunk");
                exit(1);
            }
            printf("[Client] Sent second chunk\n");
        }

        free(buf1);
        free(buf2);
        return;

    } else if (check == 4) {
        // Full file transfer
        printf("[Client] File doesen't exists. Sending the Whole file...\n");
        uint32_t fsz = htonl((uint32_t)st.st_size);
        if (sendn(sock, &fsz, sizeof fsz) != sizeof fsz) {
            perror("Failed to send full file size");
            exit(1);
        }

        FILE *fp = fopen(path, "rb");
        if (!fp) {
            perror("fopen");
            exit(1);
        }

        char buf[1024];
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, fp)) > 0) {
            if (sendn(sock, buf, r) != r) {
                perror("Failed to send file data");
                fclose(fp);
                exit(1);
            }
        }
        fclose(fp);
    }
}

//received from the server all files names to check it 
int receivedFiles(int sock, char missing_files[files_size][length_files_size], char *path){
    int count = 0;
    
    uint8_t n = 8;
    sendn(sock, &n, sizeof(n));

    while (1){
        uint32_t SizeofName;
        if (readn(sock, &SizeofName, sizeof(SizeofName)) != sizeof(SizeofName))
        {
            perror("Failed to read file name size");
            return -1;
        }
        //If the server sends a large fn, this can overflow the stack.
        uint32_t fn = ntohl(SizeofName);

        if (fn == 0){
            printf("[Client] Done Received all files From Server\n");
            return count;
        }
            

        char FileName[fn + 1];
        if (readn(sock, FileName, fn) != fn){
            perror("Failed to read file name");
            return -1;
        }
        FileName[fn] = '\0';

        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath,MAX_PATH_LEN,"%s/%s",path,FileName);

        FILE *f = fopen(fullpath, "rb");
        if (f){
            fclose(f);
            printf("[Client] %s already exist.\n", FileName);
        }
        else{
            printf("[Client] %s isn't here i will bring it, just wait few seconds :)\n", FileName);

            strncpy(missing_files[count], FileName, 256 - 1);
            missing_files[count][256 - 1] = '\0';
            count+=1;
        }

    }

    
}

//from function name its if there are any subdir new from the srver
void create_subdirs_if_needed(const char *filepath) {
    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, filepath, MAX_PATH_LEN - 1);
    path_copy[MAX_PATH_LEN - 1] = '\0';

    char *dir_path = dirname(path_copy);
    char temp[MAX_PATH_LEN];
    snprintf(temp, sizeof(temp), "%s", dir_path);

    char partial[MAX_PATH_LEN] = "";
    char *token = strtok(temp, "/");
    while (token) {
        strcat(partial, token);
        strcat(partial, "/");
        mkdir(partial, 0777); // Ignore if already exists
        token = strtok(NULL, "/");
    }
}

//received the new files or backup files
int syncWithServer(int sock, char *path){
    char missing_files[files_size][length_files_size];
    int count = receivedFiles(sock, missing_files, path);

    if(count == 0){
        printf("[Client] There is no new file from Server\n");
        uint8_t req = 0;
        if (sendn(sock, &req, sizeof(req)) != sizeof(req)) {
            perror("Failed to send 0 flag to server");
            return -1;
        }
        return 0;
    }

    printf("[Client] List of missing files:\n");
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s\n", i + 1, missing_files[i]);
    }

    for (int i = 0; i < count; i++) {
        uint8_t req = 9;
        if (sendn(sock, &req, sizeof(req)) != sizeof(req)) {
            perror("[Client] Failed to send 9 flag to server");
            return -1;
        }
        //printf("[Client] Sending To Server flag: %u\n", req);

        uint32_t fn = strlen(missing_files[i]);
        //printf("[Client] Sending To Server file name length: %u\n", fn);
        //printf("[Client] Sending To Server file name: %s\n", missing_files[i]);
        uint32_t send_len = htonl(fn);
        if (sendn(sock, &send_len, sizeof(send_len)) != sizeof(send_len)) {
            perror("Failed to send file name length");
            return -1;
        }
        if (sendn(sock, missing_files[i], fn) != fn) {
            perror("[Client] Failed to send file name");
            return -1;
        }

        uint32_t netSize;
        if (readn(sock, &netSize, sizeof(netSize)) != sizeof(netSize)) {
            perror("[Client] Failed to read the file size, Get out :(\n");
            return -1;
        }

        uint32_t fsize = ntohl(netSize);
        //printf("[Client] Received From Server file size: %u\n", fsize);
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, MAX_PATH_LEN, "%s/%s", path, missing_files[i]);
        //printf("[Client] fullpath for new file: %s\n", fullpath);

        create_subdirs_if_needed(fullpath);

        FILE *f = fopen(fullpath, "wb");
        if (!f) {
            perror("[Client] Failed to open file for writing");
            return -1;
        }

        char buf[1024];
        size_t received = 0;
        while (received < fsize) {
            size_t to_read = (fsize - received) < sizeof(buf) ? (fsize - received) : sizeof(buf);
            ssize_t r = readn(sock, buf, to_read);
            if (r <= 0) {
                perror("[Client] Failed to read file data");
                fclose(f);
                return -1;
            }
            fwrite(buf, 1, r, f);
            received += r;
        }
        fclose(f);
        printf(" [Client] Received file: %s (%u bytes)\n", missing_files[i], fsize);
    }

    uint8_t req = 0;
    if (sendn(sock, &req, sizeof(req)) != sizeof(req)) {
        perror("[Client] Failed to send final 0 flag to server");
        return -1;
    }

    return 0;
}

void traversDir(int sock, const char *dir) {
    // open the dir to travers it
    DIR *d = opendir(dir);
    if (!d) {
        perror("opendir"); 
        exit(1);
    }
    // point to a dirctory or a file 
    struct dirent *e;
    // read file by file
    while ((e = readdir(d)) != NULL) {
        // the two lines after this line, is AI generated. To prevnt infinite loops or stack overflow duo the recusive calls.
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;

        else if (e->d_type == DT_REG) {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            send_file(sock, p);
        }

        else if (e->d_type == DT_DIR) {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);

            uint8_t dirFlag = 2;
            if (sendn(sock, &dirFlag, sizeof dirFlag) != sizeof dirFlag) {
                perror("Failed to send directory flag");
                exit(1);
            }

            send_dirname(sock, e->d_name);
            traversDir(sock, p); // travers the dir files
        }
    }

    closedir(d);

    uint8_t end = 0;
    if (sendn(sock, &end, sizeof end) != sizeof end) {
        perror("Failed to send end flag");
        exit(1);
    }
}

int sendInfo(int sock, const char *usrPas) {
    uint32_t len = strlen(usrPas);
    uint32_t len_n = htonl(len);
    if (sendn(sock, &len_n, sizeof(len_n)) != sizeof(len_n)) 
    return -1;
    if (sendn(sock, usrPas, len) != len) 
    return -1;
    return 1;
}

int checkValidlity(int sock, char *dir){
    char username[20];
    char pass[20];
    uint32_t len,len_n;
    uint8_t flag;

    printf("[Client] Enter Your Username: (1-20 character) \n-->: ");
    scanf("%19s",username);

    int usr = sendInfo(sock, username);
    if(usr != 1){
        perror("cant send the username to server correctly");
        exit(1);
    }

    printf("[Client] Enter Your password: (1-20 character)\n-->: ");
    scanf("%19s",pass);

    int p = sendInfo(sock, pass);
    if(p != 1){
        perror("cant send the password to server correctly");
        exit(1);
    }

    printf("[Client] Sending directory: %s\n", dir);
    send_dirname(sock,dir);

     // — Read server’s 1‐byte auth result —
    if (readn(sock, &flag, 1) != 1) {
        perror("[Client] Failed to read auth result");
        return -1;
    }

    return (int)flag;

}

int main(int argc, char **argv){

    signal(SIGINT,  handle_signal);
    signal(SIGTSTP, handle_signal);
    

    if(argc<2){
        perror("User folder is missing.");
        exit(1);
    }
    
    char *local = argv[1];
    // the next line is AI generated to extract the dirctory name only without (/)
    const char *dest = (argc>2?argv[2]:strrchr(local,'/')?strrchr(local,'/')+1:local);

    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){perror("socket");exit(1);}
    struct sockaddr_in srv;
    fill_addr(&srv);
    if(connect(sock,(struct sockaddr*)&srv,sizeof srv)<0){
        perror("connect");exit(1);
    }

    int check=checkValidlity(sock,local);
    // if the username and pass valid 
    if(check == 1){ 
        // send the name of the server dir
        send_dirname(sock,dest);

        traversDir(sock,local);

        printf("[Client] Sync with Server Now ... \n");
        syncWithServer(sock,local);

    }
    // if the usr logging from other device
    else if(check == 2){
        printf("[Client] User is already syncing on another client. Try again later.\n");
        close(sock);
        return 0;
    }
    // non valid usr or pass
    else{
        printf("[Client] Authentication failed\n");
        close(sock);
        return 0;
    }
    


    close(sock);
    return 0;
}