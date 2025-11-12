#define _POSIX_C_SOURCE 200809L

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
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_ADDR "127.0.0.1"
#define PORT1 4001
#define PORT2 4002
#define PORT3 4003
#define BUF_SIZE 512
#define INTERVAL_MS 20

#define OPT_ENABLED 14
#define OPT_AMP 170
#define OPT_FREQ 255
#define OPT_GLITCH

static volatile int running = 1;

int write_parameter(uint16_t object, uint16_t property, uint16_t value)
{
    uint16_t msg[4];
    size_t msg_len = 4 * sizeof(uint16_t);

    // Operation = 2 (write)
    msg[0] = htons(2);
    msg[1] = htons(object);
    msg[2] = htons(property);
    msg[3] = htons(value);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send the message
    ssize_t sent = sendto(sockfd, msg, msg_len, 0,
                          (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent < 0) {
        perror("sendto");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

long long timespec_to_ms_epoch(const struct timespec *ts) {
    return (long long)ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

long long timespec_to_ms_monotonic(const struct timespec *ts) {
    return (long long)ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int connect_port(int port) {
    int sock;
    struct sockaddr_in addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr) != 1) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    if (set_nonblocking(sock) < 0) {
        perror("set_nonblocking");
    }
    return sock;
}

pthread_mutex_t state_mtx = PTHREAD_MUTEX_INITIALIZER;
char last_val[3][BUF_SIZE];
long long last_recv_monotonic[3];

// socket fds
int sfd[3];

// Reader thread: monitors sockets and updates last_val and last_recv_monotonic
void *reader_thread(void *arg) {
    (void)arg;
    fd_set rfds;
    int maxfd = sfd[0];
    if (sfd[1] > maxfd) maxfd = sfd[1];
    if (sfd[2] > maxfd) maxfd = sfd[2];

    char rbuf[BUF_SIZE];

    char partial[3][BUF_SIZE];
    memset(partial, 0, sizeof(partial));

    while (running) {
        FD_ZERO(&rfds);
        for (int i = 0; i < 3; ++i) FD_SET(sfd[i], &rfds);


        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (ready == 0) continue;

        for (int i = 0; i < 3; ++i) {
            if (!FD_ISSET(sfd[i], &rfds)) continue;
            ssize_t n = read(sfd[i], rbuf, sizeof(rbuf) - 1);
            if (n > 0) {
                rbuf[n] = '\0';
                
                strncat(partial[i], rbuf, sizeof(partial[i]) - strlen(partial[i]) - 1);

                char *search = partial[i];
                char *last_line_start = NULL;
                for (;;) {
                    char *nl = strchr(search, '\n');
                    if (!nl) break;
                    *nl = '\0';
                    last_line_start = search;
                    search = nl + 1;
                }
                if (last_line_start) {
                    // last_line_start points to start of last full line in partial[i]
                    // copy it into last_val under lock
                    struct timespec now;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    long long now_ms = timespec_to_ms_monotonic(&now);

                    pthread_mutex_lock(&state_mtx);

                    char *start = last_line_start;
                    while (*start && (*start == '\r' || *start == '\n' || *start == ' ' || *start == '\t')) start++;
                    char *end = start + strlen(start) - 1;
                    while (end > start && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) { *end = '\0'; end--; }
                    strncpy(last_val[i], start, BUF_SIZE - 1);
                    last_val[i][BUF_SIZE - 1] = '\0';
                    last_recv_monotonic[i] = now_ms;
                    pthread_mutex_unlock(&state_mtx);

                    // remove processed part from partial: everything up to 'search' remains
                    if (*search) {
                        memmove(partial[i], search, strlen(search) + 1);
                    } else {
                        partial[i][0] = '\0';
                    }
                } else {
                    // no newline yet - keep partial as is (but cap length)
                    if (strlen(partial[i]) > BUF_SIZE - 2) partial[i][BUF_SIZE - 1] = '\0';
                }
            } else if (n == 0) {
                usleep(10000);
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // nothing to read
                    continue;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    usleep(10000);
                }
            }
        }
    }

    return NULL;
}

// Printer thread: sleeps until each absolute INTERVAL_MS tick and prints JSON.
void *printer_thread(void *arg) {
    (void)arg;
    struct timespec now_mon, next_mon;
    clock_gettime(CLOCK_MONOTONIC, &now_mon);

    // Align next_mon to next 100 ms tick
    long long now_ms = timespec_to_ms_monotonic(&now_mon);
    long long ticks = now_ms / INTERVAL_MS;
    long long next_tick_ms = (ticks + 1) * INTERVAL_MS;
    next_mon.tv_sec = next_tick_ms / 1000;
    next_mon.tv_nsec = (next_tick_ms % 1000) * 1000000;

    while (running) {
        // sleep until next_mon (absolute)
        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_mon, NULL);
        } while (rc == EINTR && running);
        if (!running) break;

        struct timespec wake_mon, wake_real;
        clock_gettime(CLOCK_MONOTONIC, &wake_mon);
        clock_gettime(CLOCK_REALTIME, &wake_real);
        long long wake_mon_ms = timespec_to_ms_monotonic(&wake_mon);
        long long timestamp_epoch_ms = timespec_to_ms_epoch(&wake_real);

        // decide each out value: if last_recv_monotonic within [next_mon - INTERVAL_MS, next_mon) ??? 
        // As spec: "If no value is received from a server port within a given 100ms period, the value for that port should be \"--\"."
        // We'll consider 'within the last INTERVAL_MS' relative to this tick.
        char out1[BUF_SIZE], out2[BUF_SIZE], out3[BUF_SIZE];
        pthread_mutex_lock(&state_mtx);
        for (int i = 0; i < 3; ++i) {
            long long recv = last_recv_monotonic[i]; // monotonic ms when last value arrived
            if (recv != 0 && (wake_mon_ms - recv) <= INTERVAL_MS) {
                // value arrived within last 100 ms
                if (i == 0) strncpy(out1, last_val[i], BUF_SIZE - 1);
                if (i == 1) strncpy(out2, last_val[i], BUF_SIZE - 1);
                if (i == 2) strncpy(out3, last_val[i], BUF_SIZE - 1);
            } else {
                if (i == 0) strncpy(out1, "--", BUF_SIZE - 1);
                if (i == 1) strncpy(out2, "--", BUF_SIZE - 1);
                if (i == 2) strncpy(out3, "--", BUF_SIZE - 1);
            }
        }
        pthread_mutex_unlock(&state_mtx);

        out1[BUF_SIZE - 1] = '\0';
        out2[BUF_SIZE - 1] = '\0';
        out3[BUF_SIZE - 1] = '\0';

        double out3_val = atof(out3);

        //Server informs values for out1 have the range [50, 2000] so changed the parameters from task spec to 1000 and 2000 respectively

        static int last_state = -1; // -1 = unknown, 0 = below 3.0, 1 = above or equal 3.0

        if (strcmp(out3, "--") != 0) {
            out3_val = atof(out3);
            int current_state = (out3_val >= 3.0) ? 1 : 0;

            if (current_state != last_state) {
                if (current_state == 1) {
                    // Transition: crossed above or equal to 3.0
                    //write_parameter(1, 255, 8000);
                    write_parameter(1, 255, 1000);
                    write_parameter(1, 170, 8000);
                } else {
                    // Transition: dropped below 3.0
                    //write_parameter(1, 255, 4000);
                    write_parameter(1, 255, 2000);
                    write_parameter(1, 170, 4000);
                }
                last_state = current_state;
            }
        }

        printf("{\"timestamp\": %lld, \"out1\": \"%s\", \"out2\": \"%s\", \"out3\": \"%s\"}\n",
               timestamp_epoch_ms, out1, out2, out3);
        fflush(stdout);

        // advance next_mon by INTERVAL_MS
        long long next_ms = next_mon.tv_sec * 1000LL + next_mon.tv_nsec / 1000000LL + INTERVAL_MS;
        next_mon.tv_sec = next_ms / 1000;
        next_mon.tv_nsec = (next_ms % 1000) * 1000000;
    }

    return NULL;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    sfd[0] = connect_port(PORT1);
    sfd[1] = connect_port(PORT2);
    sfd[2] = connect_port(PORT3);
    if (sfd[0] < 0 || sfd[1] < 0 || sfd[2] < 0) {
        fprintf(stderr, "Failed to connect to all ports\n");
        return 1;
    }

    pthread_mutex_lock(&state_mtx);
    for (int i = 0; i < 3; ++i) {
        strncpy(last_val[i], "--", BUF_SIZE - 1);
        last_val[i][BUF_SIZE - 1] = '\0';
        last_recv_monotonic[i] = 0;
    }
    pthread_mutex_unlock(&state_mtx);

    pthread_t rthr, pthr;
    if (pthread_create(&rthr, NULL, reader_thread, NULL) != 0) {
        perror("pthread_create reader");
        return 1;
    }
    if (pthread_create(&pthr, NULL, printer_thread, NULL) != 0) {
        perror("pthread_create printer");
        running = 0;
        pthread_join(rthr, NULL);
        return 1;
    }

    while (running) {
        sleep(1);
    }

    pthread_cancel(rthr);
    pthread_cancel(pthr);
    pthread_join(rthr, NULL);
    pthread_join(pthr, NULL);

    for (int i = 0; i < 3; ++i) close(sfd[i]);
    return 0;
}
