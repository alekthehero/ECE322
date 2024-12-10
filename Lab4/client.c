#include "client.h"
#include "definitions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <termios.h>

void handleServerMessage(int server_fd);

void handleUserInput(int server_fd);

char *username;
char *room;

void startClient() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct pollfd fds[2];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }

    fds[0].fd = sock;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    printf("Connected to server\n");

    while (1) {
        int activity = poll(fds, 2, -1);

        if (activity < 0) {
            perror("poll error");
            exit(EXIT_FAILURE);
        }

        if (fds[0].revents & POLLIN) {
            handleServerMessage(sock);
        }

        if (fds[1].revents & POLLIN) {
            handleUserInput(sock);
        }
    }
}

void handleServerMessage(int server_fd) {
    ResponsePacket responsePacket;
    long valread = read(server_fd, (void *) &responsePacket, sizeof(ResponsePacket));
    if (valread == 0) {
        printf("Server disconnected\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    switch (responsePacket.response_type) {
        case LOGIN_SUCCESS:
            username = strdup(responsePacket.message);
            printf("\033[0;32m*> %s logged in\033[0m\n", responsePacket.message);
            break;
        case LOGIN_FAILURE:
            printf("\033[0;31m*> %s\033[0m\n", responsePacket.message);
            break;
        case CREATE_SUCCESS:
            room = strdup(responsePacket.message);
            printf("\033[0;32m*> Joined Room: %s\033[0m\n", responsePacket.message);
            break;
        case CREATE_FAILURE:
            printf("\033[0;31m*> %s\033[0m\n", responsePacket.message);
            break;
        case MESSAGE_SUCCESS:
            printf("\033[0;33m%s> %s\033[0m", responsePacket.username, responsePacket.message);
            break;
        case ROOM_JOIN:
            printf("\033[0;32m*> %s joined the room\033[0m\n", responsePacket.username);
            break;
        case ROOM_LEAVE:
            printf("\033[0;31m*> %s left the room\033[0m\n", responsePacket.username);
            break;
        case WHO_RESPONSE:
            printf("\033[0;34m*> %s -> %s\033[0m\n", responsePacket.username, responsePacket.message);
            break;
        case SHUTDOWN:
            printf("\033[0;31m*> Server shutting down\033[0m\n");
            close(server_fd);
            exit(EXIT_SUCCESS);
        default:
            printf("%s\n", "*> Invalid response");
            break;
    }
}

void handleUserInput(int server_fd) {
    char buffer[MAX_INPUT_SIZE] = {0};
    read(STDIN_FILENO, buffer, MAX_INPUT_SIZE);
    char *original = strdup(buffer);
    char *token = strtok(buffer, " \n");
    Packet packet;
    if (token != NULL) {
        if (strcmp(token, "login") == 0) {
            packet.request_type = LOGIN;
            token = strtok(NULL, " \n");
            if (token != NULL) {
                strncpy(packet.username, token, MAX_USERNAME_SIZE);
                printf("Enter password: ");
                fflush(stdout);
                char password[MAX_MESSAGE_SIZE];
                struct termios old_term, new_term;
                tcgetattr(STDIN_FILENO, &old_term);
                new_term = old_term;
                new_term.c_lflag &= ~(ECHO);
                new_term.c_lflag &= ~(ICANON);
                new_term.c_cc[VMIN] = 1;
                new_term.c_cc[VTIME] = 0;
                tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
                size_t i = 0;
                char c;
                while (i < MAX_MESSAGE_SIZE - 1) {
                    read(STDIN_FILENO, &c, 1);
                    if (c == '\n') {
                        break;
                    } else if (c == '\b' || c == 127) {
                        if (i > 0) {
                            printf("\b \b");
                            fflush(stdout);
                            i--;
                        }
                    } else {
                        password[i++] = c;
                        printf("*");
                        fflush(stdout);
                    }
                }
                password[i] = '\0';
                printf("\n");

                strncpy(packet.message, password, MAX_MESSAGE_SIZE);
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
            } else {
                printf("\033[0;31m*> Invalid Command\033[0m\n");
                return;
            }
        } else if (strcmp(token, "create") == 0) {
            packet.request_type = CREATE;
            token = strtok(NULL, " \n");
            if (token != NULL) {
                strncpy(packet.username, username, MAX_USERNAME_SIZE);
                strncpy(packet.message, token, MAX_MESSAGE_SIZE);
            } else {
                printf("\033[0;31m*> Invalid Command\033[0m\n");
                return;
            }
        } else if (strcmp(token, "enter") == 0) {
            packet.request_type = ENTER;
            token = strtok(NULL, " \n");
            if (token != NULL) {
                strncpy(packet.username, username, MAX_USERNAME_SIZE);
                strncpy(packet.message, token, MAX_MESSAGE_SIZE);
            } else {
                printf("\033[0;31m*> Invalid Command\033[0m\n");
                return;
            }
        } else if (strcmp(token, "who") == 0) {
            packet.request_type = WHO;
            send(server_fd, &packet, sizeof(Packet), 0);
            return;
        } else if (strcmp(token, "logout") == 0) {
            close(server_fd);
            exit(EXIT_SUCCESS);
        } else
            packet.request_type = MESSAGE;
    } else {
        printf("\033[0;31mInvalid Command\033[0m\n");
        return;
    }

    if (packet.request_type == MESSAGE) {
        if (username == NULL) {
            printf("\033[0;31m*> You must login first\033[0m\n");
            return;
        }
        if (room == NULL) {
            printf("\033[0;31m*> You must create or join a room first\033[0m\n");
            return;
        }
        strcpy(packet.username, username);
        strcpy(packet.message, original);
        send(server_fd, &packet, sizeof(Packet), 0);
        return;
    }

    send(server_fd, &packet, sizeof(Packet), 0);
}