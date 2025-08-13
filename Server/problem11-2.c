#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>    // for uint16_t
#include <arpa/inet.h> // for htons()

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <hex-number>\n", argv[0]);
        return 1;
    }

    uint16_t addr;
    sscanf(argv[1], "%hx", &addr); // read hex as 16-bit

    uint16_t net_order = htons(addr);

    printf("Host byte order: %hu\n", addr);
    printf("Network byte order: %hu\n", net_order);       // as unsigned short
    printf("Network byte order (hex): %#x\n", net_order); // as hex

    return 0;
}
