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
#include "utils.h"

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

int starts_with(const char *a, const char *b)
{
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("./server <port>\n");
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    short port = (short)atoi(argv[1]);


    // starting to configure tcp socket
    int socket_tcp;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_tcp < 0) {
        printf("TCP socket failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_tcp, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind to TCP port failed\n");
        return 1;
    }
    listen(socket_tcp, 50);
    // ending tcp socket configuration


    // starting to configure udp socket
    int socket_udp;
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (socket_udp < 0) {
        printf("UDP socket failed\n");
        return 1;
    }

    if (bind(socket_udp, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind to UDP failed\n");
        return 1;
    }
    // ending udp socket configuration


    struct pollfd poll_fds[53];
    int no_fds = 3;

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = socket_tcp;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = socket_udp;
    poll_fds[2].events = POLLIN;

    struct subscriber subscribers[100];
    int no_subscribers = 0;

    while (1) {
        poll(poll_fds, no_fds, -1);

        if ((poll_fds[0].revents & POLLIN) != 0) {
            // keyboard input

            char msg[10];
            scanf("%s", msg);
            if (strcmp(msg, "exit") == 0) {
                for (int i = 1; i < no_fds; i++) {
                    close(poll_fds[i].fd);
                }
                return 0;
            }

        } else if ((poll_fds[1].revents & POLLIN) != 0) {
            // tcp request

            int socket_client;
            struct sockaddr_in client_addr;
            int client_size = sizeof(client_addr);

            memset(&client_addr, 0, sizeof(client_addr));
            
            socket_client = accept(socket_tcp, (struct sockaddr*)&client_addr, &client_size);

            int flag = 1;
            setsockopt(socket_client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

            if (socket_client < 0) {
                printf("Can't accept client\n");
                continue;
            }

            poll_fds[no_fds].fd = socket_client;
            poll_fds[no_fds].events = POLLIN;
            no_fds++;

            char client_id[11] = {'\0'};
            int size;
            recv_whole_msg(socket_client, client_id, &size);

            int already_connected = 0;
            for (int i = 0; i < no_subscribers; i++) {
                if (strcmp(subscribers[i].id, client_id) == 0 && subscribers[i].fd != -1) {
                    printf("Client %s already connected.\n", client_id);
                    already_connected = 1;
                    no_fds--;
                    close(socket_client);
                }
            } 

            if (already_connected == 0 ) {
                printf("New client %s connected from %s:%d\n", client_id, inet_ntoa(client_addr.sin_addr), client_addr.sin_port);

                int reconnected = 0;
                for (int i = 0; i < no_subscribers; i++) {
                    if (strcmp(subscribers[i].id, client_id) == 0) {
                        subscribers[i].fd = socket_client;
                        reconnected = 1;

                        for (int j = 0; j < subscribers[i].no_missed_topics; j++) {
                            struct topic temp = subscribers[i].missed_topics[j];
                            char buf[1552 + 6] = {'\0'};

                            memmove(buf, &temp.ip, 4);
                            memmove(buf + 4, &temp.port, 2);
                            memmove(buf + 6, temp.content, temp.length);

                            send_whole_msg(socket_client, buf, temp.length + 6);
                        }
                        subscribers[i].no_missed_topics = 0;

                    }
                } 
                if (reconnected == 0) {
                    struct subscriber new_subscriber;
                    memmove(&new_subscriber.id, client_id, 11);
                    new_subscriber.fd = socket_client;
                    new_subscriber.no_subscribed_topics = 0;
                    new_subscriber.no_missed_topics = 0;

                    subscribers[no_subscribers] = new_subscriber;
                    no_subscribers++;
                }
            }

        } else if ((poll_fds[2].revents & POLLIN) != 0) {
            // udp request

            char buf[1552];
            int len = 0;

            struct sockaddr_in udp_client;
            memset(&udp_client, 0, sizeof(udp_client));
            int len2 = sizeof(udp_client);

            int len3 = recvfrom(poll_fds[2].fd, buf, 1552, MSG_WAITALL, (struct sockaddr *)&udp_client, &len2);

            char topic_name[51] = {'\0'};
            memmove(topic_name, buf, 50);          

            for (int i = 0; i < no_subscribers; i++) {
                if (subscribers[i].fd == -1) {
                    for (int j = 0; j < subscribers[i].no_subscribed_topics; j++) {
                        struct subscriber temp = subscribers[i];
                        if (subscribers[i].subscribed_topics[j].sf && strcmp(subscribers[i].subscribed_topics[j].name, topic_name) == 0) {
                            struct topic saved_topic;
                            memmove(saved_topic.name, buf, 50);
                            memmove(saved_topic.content, buf, len3);
                            saved_topic.length = len3;
                            saved_topic.ip = udp_client.sin_addr.s_addr;
                            saved_topic.port = udp_client.sin_port;


                            subscribers[i].missed_topics[temp.no_missed_topics] = saved_topic;
                            subscribers[i].no_missed_topics++;
                        }
                    }
                    continue;
                }
                for (int j = 0; j < subscribers[i].no_subscribed_topics; j++) {
                    struct topic current_topic = subscribers[i].subscribed_topics[j];
                    if (strcmp(topic_name, current_topic.name) == 0) {
                        char buf_to_send[1552 + 6] = {'\0'};
                        memmove(buf_to_send, &udp_client.sin_addr, 4);
                        memmove(buf_to_send + 4, &udp_client.sin_port, 2);
                        memmove(buf_to_send + 6, buf, len3);

                        send_whole_msg(subscribers[i].fd, buf_to_send, len3 + 6);
                    }
                }
            }


        
        } else {
            // request from subscriber
            for (int i = 3; i < no_fds; i++) {
                if ((poll_fds[i].revents & POLLIN) != 0) {
                    // found subscriber

                    char buf[100] = {'\0'};
                    int size;
                    recv_whole_msg(poll_fds[i].fd, buf, &size);

                    if (size == 0) {
                        // disconecting subscriber
                        
                        for (int j = 0; j < no_subscribers; j++) {
                            if (poll_fds[i].fd == subscribers[j].fd) {
                                printf("Client %s disconnected.\n", subscribers[j].id);
                                subscribers[j].fd = -1;
                                break;
                            }
                        }

                        close(poll_fds[i].fd);
                        memmove(poll_fds + i, poll_fds + i + 1, no_fds - i);
                        no_fds--;

                        

                    } else {
                        if (starts_with(buf, "subscribe")) {
                            char *topic_name = buf + 10;
                            for (int j = 0; j < no_subscribers; j++) {
                                if (subscribers[j].fd == poll_fds[i].fd) {
                                    // found who requested to subscribe

                                    struct topic new_topic;

                                    char *token = strtok(topic_name, " ");
                                    memset(new_topic.name, 0, 51);
                                    memmove(new_topic.name, token, strlen(token));

                                    token = strtok(NULL, " ");

                                    new_topic.sf = atoi(token);

                                    subscribers[j].subscribed_topics[subscribers[j].no_subscribed_topics] = new_topic;
                                    subscribers[j].no_subscribed_topics++;
                                    break;
                                }
                            }
                        } else if (starts_with(buf, "unsubscribe")) {
                            char *topic_name = buf + 12;
                            for (int j = 0; j < no_subscribers; j++) {
                                if (subscribers[j].fd == poll_fds[i].fd) {
                                    int count = 0;
                                    int total = subscribers[j].no_subscribed_topics;
                                    while (count < total) {
                                        if (strcmp(topic_name, subscribers[j].subscribed_topics[count].name) == 0) {
                                            memmove(subscribers[j].subscribed_topics + count, subscribers[j].subscribed_topics + count + 1, sizeof(struct topic) * (total - count - 1));
                                            subscribers[j].no_subscribed_topics--;
                                            break;
                                        }
                                        count++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}