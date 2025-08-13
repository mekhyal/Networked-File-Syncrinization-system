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
#include <zlib.h>
<<<<<<< HEAD
#include <libgen.h>     // for dirname
=======
#include <libgen.h> // for dirname
>>>>>>> 25df3e6 (TestCode)
#include <signal.h>

#define files_size 100
#define length_files_size 256
#define MAX_PATH_LEN 512

#define IP "127.0.0.1"
#define PORT 4599

<<<<<<< HEAD

//signal handler (CTRL + C & CTRL + Z)
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n[Client] Caught (Ctrl+C) Continuing program...\n");
    }
    else if (sig == SIGTSTP) {
        printf("\n[Clinet] Caught (Ctrl+Z) Continuing program instead of suspending...\n");
=======
// updated
void handle_signal(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n[Client] Caught (Ctrl+C)! Continuing program...\n");
    }
    else if (sig == SIGTSTP)
    {
        printf("\n[Clinet] Caught (Ctrl+Z)! Continuing program instead of suspending...\n");
>>>>>>> 25df3e6 (TestCode)
    }
    fflush(stdout);
}

<<<<<<< HEAD
void fill_addr(struct sockaddr_in *a) {
    memset(a,0,sizeof *a);
=======
void fill_addr(struct sockaddr_in *a)
{
    memset(a, 0, sizeof *a);
>>>>>>> 25df3e6 (TestCode)
    a->sin_family = AF_INET;
    a->sin_port = htons(PORT);
    if (!inet_aton(IP, &a->sin_addr))
    {
        fprintf(stderr, "invalid IP\n");
        exit(1);
    }
}

ssize_t sendn(int fd, const void *buf, size_t n)
{
    size_t sent = 0;
    const char *p = buf;
    while (sent < n)
    {
        ssize_t r = send(fd, p + sent, n - sent, 0);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            perror("send");
            exit(1);
        }
        sent += r;
    }
    return sent;
}

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n)
{
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

void send_dirname(int sock, const char *name)
{
    uint32_t len = (uint32_t)strlen(name);
    uint32_t nl = htonl(len);
    sendn(sock, &nl, sizeof nl);
    sendn(sock, name, len);
}

void send_file(int sock, const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0)
    {
        perror("stat");
        exit(1);
    }

    uint8_t flag = 1;
    sendn(sock, &flag, sizeof flag);

    const char *bn = strrchr(path, '/');
    bn = bn ? bn + 1 : path;

    uint32_t fnl = htonl((uint32_t)strlen(bn));
    sendn(sock, &fnl, sizeof fnl);
    sendn(sock, bn, strlen(bn));

    uint8_t check;
    readn(sock, &check, 1);
    printf("[Client] Received From Server flag: %u\n", check);

    if (check == 3)
    {
        // Server says file exists, send CRCs
        printf("[Client] File exists. Sending CRCs...\n");

        uint32_t fsize = (uint32_t)st.st_size;
        uint32_t first_half = fsize / 2;
        uint32_t second_half = fsize - first_half;

        FILE *fp = fopen(path, "rb");
        if (!fp)
        {
            perror("fopen");
            exit(1);
        }

        char *buf1 = malloc(first_half);
        char *buf2 = malloc(second_half);

        fread(buf1, 1, first_half, fp);
        fread(buf2, 1, second_half, fp);
        fclose(fp);

        uint32_t crc1 = crc32(0L, Z_NULL, 0);
        crc1 = crc32(crc1, (const unsigned char *)buf1, first_half);

        uint32_t crc2 = crc32(0L, Z_NULL, 0);
        crc2 = crc32(crc2, (const unsigned char *)buf2, second_half);

        // Send file size and CRCs
        uint32_t net_fsize = htonl(fsize);
        sendn(sock, &net_fsize, sizeof net_fsize);
        printf("[Client] Sending To Server file size: %u\n", fsize);
        printf("[Client] Sending To Server CRC1: %u\n", crc1);
        printf("[Client] Sending To Server CRC2: %u\n", crc2);

        uint32_t net_crc1 = htonl(crc1);
        uint32_t net_crc2 = htonl(crc2);
        sendn(sock, &net_crc1, sizeof net_crc1);
        sendn(sock, &net_crc2, sizeof net_crc2);

        // Receive update flag from server
        uint8_t update_flag;
        readn(sock, &update_flag, 1);
        printf("[Client] Server requested update flag: %u\n", update_flag);

        fp = fopen(path, "rb");
        if (!fp)
        {
            perror("fopen");
            exit(1);
        }

        if (update_flag == 5 || update_flag == 7)
        {
            fseek(fp, 0, SEEK_SET);
            fread(buf1, 1, first_half, fp);
            sendn(sock, buf1, first_half);
            printf("[Client] Sent first chunk\n");
        }

        if (update_flag == 6 || update_flag == 7)
        {
            fseek(fp, first_half, SEEK_SET);
            fread(buf2, 1, second_half, fp);
            sendn(sock, buf2, second_half);
            printf("[Client] Sent second chunk\n");
        }

        fclose(fp);
        free(buf1);
        free(buf2);
        return;
    }

    // Full file transfer
    uint32_t fsz = htonl((uint32_t)st.st_size);
    sendn(sock, &fsz, sizeof fsz);

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        perror("fopen");
        exit(1);
    }

    char buf[4096];
    size_t r;
    while ((r = fread(buf, 1, sizeof buf, f)) > 0)
        sendn(sock, buf, r);
    fclose(f);
}

<<<<<<< HEAD
//New Function
int receivedFiles(int sock,char missing_files[files_size][length_files_size],char *path){
=======
// New Function
int receivedFiles(int sock, char missing_files[files_size][length_files_size], char *path)
{
>>>>>>> 25df3e6 (TestCode)
    int count = 0;

    uint8_t n = 8;
    sendn(sock, &n, sizeof(n));

    while (1)
    {
        uint32_t SizeofName;
        if (readn(sock, &SizeofName, sizeof(SizeofName)) != sizeof(SizeofName))
        {
            perror("Failed to read file name size");
            return -1;
        }
        // If the server sends a large fn, this can overflow the stack.
        uint32_t fn = ntohl(SizeofName);

<<<<<<< HEAD
        if (fn == 0){
=======
        if (fn == 0)
        {
>>>>>>> 25df3e6 (TestCode)
            printf("[Client] Done Received all files From Server\n");
            return count;
        }

        char FileName[fn + 1];
        if (readn(sock, FileName, fn) != fn)
        {
            perror("Failed to read file name");
            return -1;
        }
        FileName[fn] = '\0';

        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, MAX_PATH_LEN, "%s/%s", path, FileName);

        FILE *f = fopen(fullpath, "rb");
        if (f)
        {
            fclose(f);
            printf("[Client] %s already exist.\n", FileName);
        }
<<<<<<< HEAD
        else{
=======
        else
        {
>>>>>>> 25df3e6 (TestCode)
            printf("[Client] %s isn't here i will bring it, just wait few seconds :)\n", FileName);

            strncpy(missing_files[count], FileName, 256 - 1);
            missing_files[count][256 - 1] = '\0';
            count += 1;
        }
    }
}

<<<<<<< HEAD
//New function
void create_subdirs_if_needed(const char *filepath) {
=======
// New function
void create_subdirs_if_needed(const char *filepath)
{
>>>>>>> 25df3e6 (TestCode)
    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, filepath, MAX_PATH_LEN - 1);
    path_copy[MAX_PATH_LEN - 1] = '\0';

    char *dir_path = dirname(path_copy);
    char temp[MAX_PATH_LEN];
    snprintf(temp, sizeof(temp), "%s", dir_path);

    char partial[MAX_PATH_LEN] = "";
    char *token = strtok(temp, "/");
    while (token)
    {
        strcat(partial, token);
        strcat(partial, "/");
        mkdir(partial, 0777); // Ignore if already exists
        token = strtok(NULL, "/");
    }
}

<<<<<<< HEAD
//There are some update
int syncWithServer(int sock, char *path){
=======
// There are some update
int syncWithServer(int sock, char *path)
{
>>>>>>> 25df3e6 (TestCode)
    char missing_files[files_size][length_files_size];
    int count = receivedFiles(sock, missing_files, path);

    if (count == 0)
    {
        printf("[Client] There is no new file from Server\n");
        uint8_t req = 0;
        sendn(sock, &req, sizeof(req));
        return 0;
    }

    printf("[Client] List of missing files:\n");
    for (int i = 0; i < count; i++)
    {
        printf("  [%d] %s\n", i + 1, missing_files[i]);
    }

    // then sends to server the file name that you want and received from them the content

    for (int i = 0; i < count; i++)
    {

        uint8_t req = 9;
        sendn(sock, &req, sizeof(req));
        printf("[Client] Sending To Server flag: %u\n", req);
        // printf(" - %s\n", missing_files[i]);
        uint32_t fn = strlen(missing_files[i]);
        printf("[Client] Sending To Server file name length: %u\n", fn);
        printf("[Client] Sending To Server file name: %s\n", missing_files[i]);
        uint32_t send_len = htonl(fn);
        sendn(sock, &send_len, sizeof(send_len));
        sendn(sock, missing_files[i], fn);

        uint32_t netSize;
        if (readn(sock, &netSize, sizeof(netSize)) != sizeof(netSize))
        {
            perror("Failed to read the file size, Get out :(\n");
            return -1;
        }

        uint32_t fsize = ntohl(netSize);
        printf("[Client] Received From Server file size: %u\n", fsize);
        char fullpath[MAX_PATH_LEN];

<<<<<<< HEAD
        snprintf(fullpath,MAX_PATH_LEN,"%s/%s",path,missing_files[i]);
        printf("[Client] fullpath for new file: %s\n",fullpath);
        //NewFunction
=======
        snprintf(fullpath, MAX_PATH_LEN, "%s/%s", path, missing_files[i]);
        printf("[Client] fullpath for new file: %s\n", fullpath);
        // NewFunction
>>>>>>> 25df3e6 (TestCode)
        //===========================================================
        create_subdirs_if_needed(fullpath);
        //======================================
        FILE *f = fopen(fullpath, "wb");
        if (!f)
        {
            perror("Failed to open file for writing");
            return -1;
        }

        char buf[1024];
        size_t received = 0;
        while (received < fsize)
        {
            size_t to_read = (fsize - received) < sizeof(buf) ? (fsize - received) : sizeof(buf);
            ssize_t r = readn(sock, buf, to_read);
            if (r <= 0)
            {
                perror("Failed to read file data");
                return -1;
            }
            fwrite(buf, 1, r, f);
            received += r;
        }
        fclose(f);
        printf("Received file: %s (%u bytes)\n", missing_files[i], fsize);
    }
    uint8_t req = 0;
    sendn(sock, &req, sizeof(req));
    return 0;
}

<<<<<<< HEAD
//the end if every dir should be send 0 flag
void recurse_send(int sock, char *folder) {
    DIR *d=opendir(folder);
    if(!d){perror("opendir");exit(1);}
=======
// the end if every dir should be send 0 flag
void recurse_send(int sock, char *folder)
{
    DIR *d = opendir(folder);
    if (!d)
    {
        perror("opendir");
        exit(1);
    }
>>>>>>> 25df3e6 (TestCode)
    struct dirent *e;

    // 1) files
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_name[0] == '.' &&
            (e->d_name[1] == '\0' ||
             (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        if (e->d_type == DT_REG)
        {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", folder, e->d_name);
            send_file(sock, p);
        }
    }
    rewinddir(d);

    // 2) subdirs
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_name[0] == '.' &&
            (e->d_name[1] == '\0' ||
             (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        if (e->d_type == DT_DIR)
        {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", folder, e->d_name);

            uint8_t dflag = 2; // <-- NEW: directory flag
            sendn(sock, &dflag, sizeof dflag);

            send_dirname(sock, e->d_name); // <-- send only basename
            recurse_send(sock, p);
        }
    }
    closedir(d);
<<<<<<< HEAD
    uint8_t end=0;
    sendn(sock,&end,sizeof end);

    
}

int client_auth_Send(int sock, char *dir){
    char username[20];
    char pass[20];
    uint32_t len,len_n;
    uint8_t flag;

    printf("[Client] Enter Your Username: (1-20 character) \n-->: ");
    scanf("%19s",username);

    len=strlen(username);
    len_n=htonl(len);

    printf("[Client] Sending to server lenght of User name\n");
    if(sendn(sock,&len_n,4) != 4){
        perror("[Client] Failed To Send length of username");
        return -1;
    }
    
    printf("[Client] Sending to server User name\n");
    if(sendn(sock,username,len) != len){
        perror("[Client] Failed To Send username");
        return -1;
    }
    

    printf("[Client] Enter Your password: (1-20 character)\n-->: ");
    scanf("%19s",pass);

    len=strlen(pass);
    len_n=htonl(len);

    printf("[Client] Sending to server lenght of Password\n");
    if(sendn(sock,&len_n,4) != 4){
        perror("[Client] Failed To Send length of Password");
        return -1;
    }
   
    printf("[Client] Sending to server Password\n");
    if(sendn(sock,pass,len) != len){
        perror("[Client] Failed To Send Password");
        return -1;
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
    //New part ===============================
    signal(SIGINT,  handle_signal);
    signal(SIGTSTP, handle_signal);
    //========================================

    if(argc<2){
        fprintf(stderr,"Usage: %s <local_dir> [remote_name]\n",argv[0]);
        exit(1);
    }
    
    char *local = argv[1];
    const char *remote = (argc>2?argv[2]:strrchr(local,'/')?strrchr(local,'/')+1:local);
=======
    uint8_t end = 0;
    sendn(sock, &end, sizeof end);
}

int client_auth_Send(int sock, char *dir)
{
    char username[20];
    char pass[20];
    uint32_t len, len_n;
    uint8_t flag;

    printf("[Client] Enter Your Username: (1-20 character) \n-->: ");
    scanf("%19s", username);

    len = strlen(username);
    len_n = htonl(len);

    printf("[Client] Sending to server lenght of User name\n");
    if (sendn(sock, &len_n, 4) != 4)
    {
        perror("[Client] Failed To Send length of username");
        return -1;
    }

    printf("[Client] Sending to server User name\n");
    if (sendn(sock, username, len) != len)
    {
        perror("[Client] Failed To Send username");
        return -1;
    }

    printf("[Client] Enter Your password: (1-20 character)\n-->: ");
    scanf("%19s", pass);

    len = strlen(pass);
    len_n = htonl(len);

    printf("[Client] Sending to server lenght of Password\n");
    if (sendn(sock, &len_n, 4) != 4)
    {
        perror("[Client] Failed To Send length of Password");
        return -1;
    }

    printf("[Client] Sending to server Password\n");
    if (sendn(sock, pass, len) != len)
    {
        perror("[Client] Failed To Send Password");
        return -1;
    }
    printf("[Client] Sending directory: %s\n", dir);
    send_dirname(sock, dir);

    // — Read server’s 1‐byte auth result —
    if (readn(sock, &flag, 1) != 1)
    {
        perror("[Client] Failed to read auth result");
        return -1;
    }

    return (int)flag;
}

int main(int argc, char **argv)
{
    // New part ===============================
    signal(SIGINT, handle_signal);
    signal(SIGTSTP, handle_signal);
    //========================================

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <local_dir> [remote_name]\n", argv[0]);
        exit(1);
    }
>>>>>>> 25df3e6 (TestCode)

    char *local = argv[1];
    const char *remote = (argc > 2 ? argv[2] : strrchr(local, '/') ? strrchr(local, '/') + 1
                                                                   : local);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in srv;
    fill_addr(&srv);
    if (connect(sock, (struct sockaddr *)&srv, sizeof srv) < 0)
    {
        perror("connect");
        exit(1);
    }

    // New Part =============================================================
<<<<<<< HEAD
    int check=client_auth_Send(sock,local);

    if(check == 1){
        // initial dir (no flag)
        send_dirname(sock,remote);

        recurse_send(sock,local);

        printf("[Client] Sync with Server\n");
        syncWithServer(sock,local);

    }else if(check == 2){
        printf("[Client] User is already syncing on another client. Try again later.\n");
        close(sock);
        return 0;
    }else{
        printf("[Client] Authentication failed\n");
        close(sock);
        return 0;
    }
    
=======
    int check = client_auth_Send(sock, local);

    if (check == 1)
    {
        // initial dir (no flag)
        send_dirname(sock, remote);

        recurse_send(sock, local);
>>>>>>> 25df3e6 (TestCode)

        printf("[Client] Sync with Server\n");
        syncWithServer(sock, local);
    }
    else if (check == 2)
    {
        printf("[Client] User is already syncing on another client. Try again later.\n");
        close(sock);
        return 0;
    }
    else
    {
        printf("[Client] Authentication failed\n");
        close(sock);
        return 0;
    }

    close(sock);
    return 0;
    //====================================================
}