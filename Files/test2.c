// server.c
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
#include <zlib.h>    // for crc32()

#define DIR_NAME       "ServerDir"
#define BASE_PATH      "/home/aziz32/223Project/Files/" DIR_NAME
#define PORT           4590
#define CRC_BUF_SIZE   (64*1024)

// readn: read exactly n bytes or return <=0 on error/EOF
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    char *p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            return 0;
        }
        left -= r;
        p += r;
    }
    return n;
}

// sendn: send exactly n bytes or exit on error
ssize_t sendn(int fd, const void *buf, size_t n) {
    size_t sent = 0;
    const char *p = buf;
    while (sent < n) {
        ssize_t r = send(fd, p + sent, n - sent, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("send");
            exit(1);
        }
        sent += r;
    }
    return n;
}

int open_listenfd(int port) {
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        perror("Socket Failed");
        exit(1);
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);
    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
        perror("Bind Failed");
        close(serverfd);
        exit(1);
    }
    if (listen(serverfd, 5) < 0) {
        perror("Listen Failed");
        close(serverfd);
        exit(1);
    }
    return serverfd;
}

// createDir: read [len,name], mkdir under parent_path
void createDir(int clientfd,
               const char *parent_path,
               char *out_fullpath,
               size_t out_n) 
{
    uint32_t net_len;
    if (readn(clientfd, &net_len, 4) != 4) {
        perror("Failed to read length");
        return;
    }
    uint32_t len = ntohl(net_len);

    char name[len+1];
    if (readn(clientfd, name, len) != (ssize_t)len) {
        perror("Failed to read directory name");
        return;
    }
    name[len] = '\0';

    snprintf(out_fullpath, out_n, "%s/%s", parent_path, name);

    struct stat st;
    if (stat(out_fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Directory already exists: %s\n", out_fullpath);
    } else if (mkdir(out_fullpath, 0755) == 0) {
        printf("Created directory: %s\n", out_fullpath);
    } else {
        perror("Error creating directory");
    }
}

// traverseDir: return 1 if filename present in dir, else 0
int traverseDir(const char *dir, const char *filename) {
    DIR *d = opendir(dir);
    if (!d) {
        perror("Error opening dir");
        exit(1);
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' &&
           (e->d_name[1] == '\0' ||
           (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        if (strcmp(e->d_name, filename) == 0) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

// stream exactly bytes_to_crc bytes from file into CRC32
uint32_t stream_crc32(FILE *f, size_t bytes_to_crc) {
    uint8_t buf[CRC_BUF_SIZE];
    uint32_t crc = crc32(0L, Z_NULL, 0);
    size_t left = bytes_to_crc;
    while (left > 0) {
        size_t chunk = left < CRC_BUF_SIZE ? left : CRC_BUF_SIZE;
        if (fread(buf, 1, chunk, f) != chunk) {
            perror("fread for CRC");
            break;
        }
        crc = crc32(crc, buf, chunk);
        left -= chunk;
    }
    return crc;
}

// split file into halves, compare to expected CRCs → return flag number
// 3 = no change, 4 = send whole file, 5 = send first half, 6 = send second half
uint8_t checksum(const char *fullpath,
                 uint32_t fsize,
                 uint32_t exp_crc1,
                 uint32_t exp_crc2)
{
    FILE *f = fopen(fullpath, "rb");
    if (!f) {
        perror("fopen");
        return 4;
    }
    uint32_t half1 = fsize / 2, half2 = fsize - half1;
    uint32_t crc1 = stream_crc32(f, half1);
    uint32_t crc2 = stream_crc32(f, half2);
    fclose(f);

    if (crc1 != exp_crc1)      return 5;
    else if (crc2 != exp_crc2) return 6;
    else                       return 3;
}

// savefile: [filename][fsize] → CRC handshake → [new data]
void savefile(int clientfd, char *base_path)
{
    // 1. Read filename length + name
    uint32_t net_fnlen;
    if (readn(clientfd, &net_fnlen, 4) != 4) {
        perror("Failed to read filename length");
        return;
    }
    uint32_t fnlen = ntohl(net_fnlen);
    char filename[fnlen+1];
    if (readn(clientfd, filename, fnlen) != (ssize_t)fnlen) {
        perror("Failed to read filename");
        return;
    }
    filename[fnlen] = '\0';

    // 2. Read file size
    uint32_t net_fsize;
    if (readn(clientfd, &net_fsize, 4) != 4) {
        perror("Failed to read file size");
        return;
    }
    uint32_t file_size = ntohl(net_fsize);

    // 3. Construct full path
    char fullpath[512];
    snprintf(fullpath, sizeof fullpath, "%s/%s", base_path, filename);

    // 4. CRC negotiation
    uint8_t reply;
    if (!traverseDir(base_path, filename)) {
        printf("File does not exist → send whole content.\n");
        reply = 4;
        sendn(clientfd, &reply, 1);
    } else {
        printf("File exists → send expected CRCs.\n");
        uint32_t net_crc1, net_crc2;
        if (readn(clientfd, &net_crc1, 4) != 4 ||
            readn(clientfd, &net_crc2, 4) != 4) {
            perror("Failed to read CRCs");
            return;
        }
        uint32_t exp1 = ntohl(net_crc1);
        uint32_t exp2 = ntohl(net_crc2);

        reply = checksum(fullpath, file_size, exp1, exp2);
        sendn(clientfd, &reply, 1);

        if (reply == 3) {
            printf("There is no changes.\n");
            return;
        }
        if (readn(clientfd, &net_fsize, 4) != 4) {
            perror("Failed to read new file size");
            return;
        }
        file_size = ntohl(net_fsize);
    }

    // 5. Receive and write file content
    FILE *fptr = fopen(fullpath, "wb");
    if (!fptr) {
        perror("Failed to open file for writing.");
        return;
    }
    size_t received = 0;
    char buf[1024];
    while (received < file_size) {
        size_t to_read = (file_size - received < sizeof buf
                          ? file_size - received
                          : sizeof buf);
        ssize_t r = readn(clientfd, buf, to_read);
        if (r <= 0) {
            perror("Failed to read file data");
            break;
        }
        fwrite(buf, 1, r, fptr);
        received += r;
    }
    fclose(fptr);
    printf("Received file: %s (%u bytes)\n", filename, file_size);
}

// recursive directory handler
void subDirectory(int clientfd, char *base_path)
{
    while (1) {
        uint8_t flag;
        if (readn(clientfd, &flag, 1) != 1) {
            perror("Failed to read flag.");
            return;
        }
        if (flag == 0) {
            printf("Client finished sending files.\n");
            return;
        }
        else if (flag == 1) {
            savefile(clientfd, base_path);
        }
        else if (flag == 2) {
            char subpath[512];
            createDir(clientfd, base_path, subpath, sizeof subpath);
            subDirectory(clientfd, subpath);
        }
        else {
            fprintf(stderr, "Unknown flag %u\n", flag);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int serverfd = open_listenfd(atoi(argv[1]));
    printf("Server listening on port %d...\n", atoi(argv[1]));

    struct stat st;
    if (stat(DIR_NAME, &st) == 0) {
        printf("Directory already exists: %s\n", DIR_NAME);
    }
    else if (mkdir(DIR_NAME, 0755) == 0) {
        printf("Empty directory created successfully: %s\n", DIR_NAME);
    }
    else {
        perror("Error creating directory");
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof client_addr;
    int clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &addrlen);
    if (clientfd < 0) {
        perror("Accept Failed");
        close(serverfd);
        return 1;
    }
    printf("Client connected.\n");

    char topath[512];
    createDir(clientfd, BASE_PATH, topath, sizeof topath);
    printf("Syncing under: %s\n", topath);

    subDirectory(clientfd, topath);

    close(clientfd);
    close(serverfd);
    return 0;
}
