#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/poll.h>
#include <netinet/tcp.h>


void send_whole_msg(int fd, char *msg, int size) {
    int quotient = size / 256;
    int remainder = size % 256;

    while (send(fd, (char*)&quotient, 1, 0) != 1);
    while (send(fd, (char*)&remainder, 1, 0) != 1);

    int remaining = size;
    while (remaining > 0) {
        int r = send(fd, msg + (size - remaining), remaining, 0);
        remaining -= r;
    }
}

void recv_whole_msg(int fd, char *buf, int *size) {
    int quotient = 0;
    recv(fd, (char*)&quotient, 1, 0);

    int remainder = 0;
    recv(fd, (char*)&remainder, 1, 0);

    int total_size = quotient * 256 + remainder;
    int current = 0;

    while (current < total_size) {
        int r = recv(fd, buf + current, total_size - current, 0);
        current += r;
    }

    *size = total_size;
}

int starts_with(const char *a, const char *b)
{
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

int main(int argc, char **argv) {
    
    if (argc != 4) {
        printf("./subscriber <id_client> <ip_server> <port_server>\n");
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int socket_client;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    socket_client = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_client < 0) {
        printf("TCP socket failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    if (connect(socket_client, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Can't connect\n");
        return 1;
    }

    int flag = 1;
    setsockopt(socket_client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    char msg[11] = {'\0'};
    memmove(msg, argv[1], 11);

    send_whole_msg(socket_client, msg, 11);

    struct pollfd poll_fds[2];

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = socket_client;
    poll_fds[1].events = POLLIN;

    while (1) {
        poll(poll_fds, 2, -1);

        if ((poll_fds[0].revents & POLLIN) != 0) {
            // keyboard
            char temp[100];

            fgets(temp, 100, stdin);
            if (temp[strlen(temp) - 1] == '\n') {
                temp[strlen(temp) - 1] = '\0';
            }

            if (starts_with(temp, "exit")) {
                close(socket_client);
                return 0;
            }

            if (starts_with(temp, "subscribe")) {
                send_whole_msg(socket_client, temp, strlen(temp));
                printf("Subscribed to topic.\n");
            } else if (starts_with(temp, "unsubscribe")) {
                send_whole_msg(socket_client, temp, strlen(temp));
                printf("Unsubscribed from topic.\n");
            }
        } else {
            // received news

            char buf[1552 + 6] = {'\0'};
            int size;
            recv_whole_msg(poll_fds[1].fd, buf, &size);

            if (size == 0) {
                close(socket_client);
                return 0;
            }

            char *actual_message = buf + 6;
            char topic[51];
            memmove(topic, actual_message, 50);
            topic[50] = '\0';

            char type = *(actual_message + 50);

            struct in_addr x;
            x.s_addr = *buf;
            short port = *(buf + 4);
            printf("%s:%d - ", inet_ntoa(x), port);

            if (type == 0) {
                char sign = *(actual_message + 51);
                int p1 = *(actual_message + 52) & 0xff;
                int p2 = *(actual_message + 53) & 0xff;
                int p3 = *(actual_message + 54) & 0xff;
                int p4 = *(actual_message + 55) & 0xff;

                int rez = p1<<24 | p2<<16 | p3<<8 | p4;

                if (sign == 1) {
                    rez = -rez;
                }
                printf("%s - INT - %d\n", topic, rez);

            } else if (type == 1) {
                int p1 = *(actual_message + 51) & 0xff;
                int p2 = *(actual_message + 52) & 0xff;
                float rez = (float)(p1<<8 | p2) / 100.0;
                printf("%s - SHORT_REAL - %.2f\n", topic, rez);

            } else if (type == 2) {
                char sign = *(actual_message + 51);

                int p1 = *(actual_message + 52) & 0xff;
                int p2 = *(actual_message + 53) & 0xff;
                int p3 = *(actual_message + 54) & 0xff;
                int p4 = *(actual_message + 55) & 0xff;

                char exp = *(actual_message + 56) & 0xff;
                char exp2 = exp;

                float rez = (float)(p1<<24 | p2<<16 | p3<<8 | p4);
                while (exp > 0) {
                    rez = rez / 10.0;
                    exp--;
                }

                if (sign == 1) {
                    rez = -rez;
                }
                printf("%s - FLOAT - %.*f\n", topic, exp2, rez);

            } else if (type == 3) {
                printf("%s - STRING - %s\n", topic, actual_message + 51);
            }
        }
    }
}