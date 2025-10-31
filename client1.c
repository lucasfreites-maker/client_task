// client1.c
// Embedded Linux Homework - Task 1
//
// Reads data from TCP ports 4001, 4002, 4003 on localhost
// Prints a JSON line every 100 ms with timestamp and latest received values
//
// Compile with:  gcc client1.c -o client1
//

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

// helper: current time in ms since epoch
long long current_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

// helper: set socket non-blocking
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

// connect to localhost:port, return socket fd
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

    while (1) {
        // prepare for select()
        FD_ZERO(&readfds);
        FD_SET(sock1, &readfds);
        FD_SET(sock2, &readfds);
        FD_SET(sock3, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 ms

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0) {
            perror("select");
            break;
        }

        // check each socket
        if (FD_ISSET(sock1, &readfds)) {
            n = read(sock1, tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                // store last line (strip newline if any)
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

        long long ts = current_timestamp_ms();

        // print JSON line
        printf("{\"timestamp\": %lld, \"out1\": \"%s\", \"out2\": \"%s\", \"out3\": \"%s\"}\n",
               ts, buf1, buf2, buf3);
        fflush(stdout);
    }

    close(sock1);
    close(sock2);
    close(sock3);
    return 0;
}
