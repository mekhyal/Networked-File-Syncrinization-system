#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <zlib.h>

// GENERATED ONLINE
uint32_t CRC(const char *data, uint32_t length) {
    return crc32(0, (const unsigned char *)data, length);
}

// sending the content based on the changed.
void checkSums(int sockFd, const char *path, const uint32_t fsize) {
    // split the file into 2 chunks
    uint32_t firstHalf = fsize / 2;
    uint32_t secondHalf = fsize - firstHalf;

    // dynamically allocate memory for the chunks
    char *chunk1 = malloc(firstHalf);
    char *chunk2 = malloc(secondHalf);
    char *fullFile = malloc(fsize);

    if (!chunk1 || !chunk2 || !fullFile) {
        perror("Memory failure");
        exit(1);
    }

    // open the file
    FILE *fptr = fopen(path, "rb");
    if (!fptr) {
        perror("Error opening file");
        free(chunk1); free(chunk2); free(fullFile);
        exit(1);
    }

    // read chunks from the file
    if (fread(chunk1, 1, firstHalf, fptr) != firstHalf ||
        fread(chunk2, 1, secondHalf, fptr) != secondHalf) {
        perror("Error reading chunks");
        fclose(fptr); free(chunk1); free(chunk2); free(fullFile);
        exit(1);
    }

    rewind(fptr);
    if (fread(fullFile, 1, fsize, fptr) != fsize) {
        perror("Error reading the whole file");
        fclose(fptr); free(chunk1); free(chunk2); free(fullFile);
        exit(1);
    }

    fclose(fptr);

    // calculate CRC checksums
    uint32_t crc1 = htonl(CRC(chunk1, firstHalf));
    uint32_t crc2 = htonl(CRC(chunk2, secondHalf));

    uint8_t changed;
    if (readn(sockFd, &changed, sizeof(changed)) != sizeof(changed)) {
        perror("Failed to receive initial flag");
        free(chunk1); free(chunk2); free(fullFile);
        exit(1);
    }

    if (changed == 7) {
        printf("sending the CRCs correctly\n");
        if (sendn(sockFd, &crc1, sizeof(crc1)) != sizeof(crc1) ||
            sendn(sockFd, &crc2, sizeof(crc2)) != sizeof(crc2)) {
            perror("Failed to send CRCs");
            free(chunk1); free(chunk2); free(fullFile);
            exit(1);
        }

        if (readn(sockFd, &changed, sizeof(changed)) != sizeof(changed)) {
            perror("Failed to receive change flag");
            free(chunk1); free(chunk2); free(fullFile);
            exit(1);
        }

        if (changed == 4) {
            uint32_t firstHalf_n = htonl(firstHalf);
            if (sendn(sockFd, &firstHalf_n, sizeof(firstHalf_n)) != sizeof(firstHalf_n) ||
                sendn(sockFd, chunk1, firstHalf) != firstHalf) {
                perror("Failed to send chunk1");
                free(chunk1); free(chunk2); free(fullFile);
                exit(1);
            }
            printf("sending first chunk correctly\n");
        } else if (changed == 5) {
            uint32_t secondHalf_n = htonl(secondHalf);
            if (sendn(sockFd, &secondHalf_n, sizeof(secondHalf_n)) != sizeof(secondHalf_n) ||
                sendn(sockFd, chunk2, secondHalf) != secondHalf) {
                perror("Failed to send chunk2");
                free(chunk1); free(chunk2); free(fullFile);
                exit(1);
            }
            printf("sending second chunk correctly\n");
        } else if (changed == 6) {
            printf("Nothing changed\n");
        } else {
            fprintf(stderr, "Unexpected change flag: %d\n", changed);
        }

        printf("Done\n");
    } else if (changed == 3) {
        if (sendn(sockFd, fullFile, fsize) != fsize) {
            perror("Failed to send the whole file content");
            free(chunk1); free(chunk2); free(fullFile);
            exit(1);
        }
        printf("sending the whole file content correctly\n");
    } else {
        fprintf(stderr, "Unexpected initial flag: %d\n", changed);
    }

    free(chunk1);
    free(chunk2);
    free(fullFile);
}
