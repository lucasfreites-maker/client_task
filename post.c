#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

// Utility function to print bytes in hex
void print_hexdump(const unsigned char *data, size_t len) {
    printf("Sent message (%zu bytes): ", len);
    for (size_t i = 0; i < len; i++)
        printf("%02X ", data[i]);
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s read <object> <property>\n", argv[0]);
        fprintf(stderr, "  %s write <object> <property> <value>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t operation = 0;
    uint16_t object = atoi(argv[2]);
    uint16_t property = atoi(argv[3]);
    uint16_t value = 0;
    uint16_t msg[4];
    size_t msg_len;

    if (strcmp(argv[1], "read") == 0) {
        operation = 1;
        msg[0] = htons(operation);
        msg[1] = htons(object);
        msg[2] = htons(property);
        msg_len = 3 * sizeof(uint16_t);
    } 
    else if (strcmp(argv[1], "write") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Write operation requires a value.\n");
            return EXIT_FAILURE;
        }
        operation = 2;
        value = atoi(argv[4]);
        msg[0] = htons(operation);
        msg[1] = htons(object);
        msg[2] = htons(property);
        msg[3] = htons(value);
        msg_len = 4 * sizeof(uint16_t);
    } 
    else {
        fprintf(stderr, "Invalid operation. Use 'read' or 'write'.\n");
        return EXIT_FAILURE;
    }

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4000);  // UDP port 4000
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send the message
    ssize_t sent = sendto(sockfd, msg, msg_len, 0,
                          (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent < 0) {
        perror("sendto");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // Print what was sent
    print_hexdump((unsigned char *)msg, msg_len);

    close(sockfd);
    printf("Message sent to 127.0.0.1:4000 successfully.\n");

    return EXIT_SUCCESS;
}