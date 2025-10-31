// client1.c (fixed timing version)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define SERVER_ADDR "127.0.0.1"
#define PORT1 4001
#define PORT2 4002
#define PORT3 4003
#define BUF_SIZE 256
#define PRINT_INTERVAL_MS 100

long long current_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int connect_port(int port) {
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    set_nonblocking(sock);
    return sock;
}

int main() {
    int sock1 = connect_port(PORT1);
    int sock2 = connect_port(PORT2);
    int sock3 = connect_port(PORT3);

    char buf1[BUF_SIZE] = "--", buf2[BUF_SIZE] = "--", buf3[BUF_SIZE] = "--";
    char tmp[BUF_SIZE];
    ssize_t n;
    fd_set readfds;
    int maxfd = sock1;
    if (sock2 > maxfd) maxfd = sock2;
    if (sock3 > maxfd) maxfd = sock3;

    struct timeval timeout;
    long long last_print = current_timestamp_ms();

    while (1) {
        // small timeout for responsiveness
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms

        FD_ZERO(&readfds);
        FD_SET(sock1, &readfds);
        FD_SET(sock2, &readfds);
        FD_SET(sock3, &readfds);

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            perror("select");
            break;
        }

        // read available data
        if (FD_ISSET(sock1, &readfds)) {
            n = read(sock1, tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                char *nl = strchr(tmp, '\n');
                if (nl) *nl = '\0';
                strncpy(buf1, tmp, sizeof(buf1) - 1);
            }
        }
        if (FD_ISSET(sock2, &readfds)) {
            n = read(sock2, tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                char *nl = strchr(tmp, '\n');
                if (nl) *nl = '\0';
                strncpy(buf2, tmp, sizeof(buf2) - 1);
            }
        }
        if (FD_ISSET(sock3, &readfds)) {
            n = read(sock3, tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                char *nl = strchr(tmp, '\n');
                if (nl) *nl = '\0';
                strncpy(buf3, tmp, sizeof(buf3) - 1);
            }
        }

        // check if it's time to print
        long long now = current_timestamp_ms();
        if (now - last_print >= PRINT_INTERVAL_MS) {
            printf("{\"timestamp\": %lld, \"out1\": \"%s\", \"out2\": \"%s\", \"out3\": \"%s\"}\n",
                   now, buf1, buf2, buf3);
            fflush(stdout);
            last_print = now;
        }
    }

    close(sock1);
    close(sock2);
    close(sock3);
    return 0;
}
