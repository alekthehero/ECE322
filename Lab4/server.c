#include "server.h"
#include "definitions.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>

typedef struct {
    char username[MAX_USERNAME_SIZE];
    char password[MAX_MESSAGE_SIZE];
} UserData;

typedef struct {
    char username[MAX_USERNAME_SIZE];
    char room[MAX_ROOM_NAME_SIZE];
    int client_fd;
} User;

FILE *file;
User users[MAX_CLIENTS];
int server_fd;
struct pollfd pollfd[MAX_CLIENTS];
int clientCount = 1; // Offset by 1 to account for server socket

void handleClient(int client_fd, int index);

void processLogin(int sender, Packet packet);

void processCreate(int sender, Packet packet);

void processMessage(int sender, Packet packet);

void processEnter(int sender, Packet packet);

void processWho(int sender, Packet packer);

void sendRoomChange(User sender, ResponseType type);

void cleanup() {
    printf("Shutting down server...\n");

    ResponsePacket response;
    response.response_type = SHUTDOWN;
    strcpy(response.message, "Server is shutting down");

    for (int i = 1; i < clientCount; i++) {
        send(pollfd[i].fd, &response, sizeof(response), 0);
        close(pollfd[i].fd);
    }

    close(server_fd);
    fclose(file);
    printf("Server shutdown done.\n");
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

int startServer() {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    file = fopen("data.dat", "a+b");

    signal(SIGINT, cleanup);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    pollfd[0].fd = server_fd;
    pollfd[0].events = POLLIN;

    printf("Server started on port %d\n", PORT);
    while (1) {
        int activity = poll(pollfd, clientCount, -1);

        if (activity < 0) {
            perror("poll error");
            exit(EXIT_FAILURE);
        }

        if (pollfd[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }

            pollfd[clientCount].fd = new_socket;
            pollfd[clientCount].events = POLLIN;
            clientCount++;
            printf("New client connected: %i\n", new_socket);
        }

        for (int i = 1; i < clientCount; i++) {
            if (pollfd[i].revents & POLLIN) {
                handleClient(pollfd[i].fd, i);
            }
        }
    }
}

void handleClient(int client_fd, int index) {
    Packet buffer;
    long valread = read(client_fd, (void *) &buffer, sizeof(Packet));
    if (valread == 0) {
        printf("Client disconnected: %i\n", pollfd[index].fd);
        close(client_fd);
        pollfd[index] = pollfd[--(clientCount)];
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].client_fd == client_fd) {
                sendRoomChange(users[i], ROOM_LEAVE);
                User newUser;
                users[i] = newUser;
            }
        }
    } else {
        switch (buffer.request_type) {
            case LOGIN:
                processLogin(client_fd, buffer);
                break;
            case CREATE:
                processCreate(client_fd, buffer);
                break;
            case ENTER:
                processEnter(client_fd, buffer);
                break;
            case WHO:
                processWho(client_fd, buffer);
                break;
            case MESSAGE:
                processMessage(client_fd, buffer);
                break;
            default:
                break;
        }
    }
}

UserData findUserData(char *username) {
    UserData user;
    fseek(file, 0, SEEK_SET);
    while (fread(&user, sizeof(UserData), 1, file) == 1) {
        if (strcmp(user.username, username) == 0) {
            return user;
        }
    }
    UserData empty;
    return empty;
}

User *findUser(char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    User empty;
    return &empty;
}

void sendInChatRoom(User sender, char roomToSendMessage[MAX_ROOM_NAME_SIZE], char message[MAX_MESSAGE_SIZE],
                    ResponseType type) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(users[i].room, roomToSendMessage) == 0) {
            ResponsePacket response;
            response.response_type = type;
            strcpy(response.username, sender.username);
            strcpy(response.message, message);
            send(users[i].client_fd, &response, sizeof(response), 0);
        }
    }
}

void sendRoomChange(User sender, ResponseType type) {
    if (sender.room[0] == '\0' && type == ROOM_LEAVE) {
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].client_fd != sender.client_fd && strcmp(users[i].room, sender.room) == 0) {
            ResponsePacket response;
            response.response_type = type;
            strcpy(response.username, sender.username);
            send(users[i].client_fd, &response, sizeof(response), 0);
        }
    }
}

void processLogin(int sender, Packet packet) {
    UserData userData = findUserData(packet.username);
    if (strlen(userData.username) == 0) {
        strcpy(userData.username, packet.username);
        strcpy(userData.password, packet.message);
        fwrite(&userData, sizeof(UserData), 1, file);
        printf("LOG> User created: %s\n", userData.username);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (users[i].username[0] == '\0') {
                strcpy(users[i].username, userData.username);
                users[i].client_fd = sender;
                break;
            }
        }
        ResponsePacket response;
        response.response_type = LOGIN_SUCCESS;
        strcpy(response.message, userData.username);
        send(sender, &response, sizeof(response), 0);
        return;
    }

    if (strcmp(userData.password, packet.message) != 0) {
        ResponsePacket response;
        response.response_type = LOGIN_FAILURE;
        strcpy(response.message, "Invalid password");
        send(sender, &response, sizeof(response), 0);
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].username[0] == '\0') {
            strcpy(users[i].username, userData.username);
            users[i].client_fd = sender;
            break;
        }
    }
    ResponsePacket response;
    response.response_type = LOGIN_SUCCESS;
    strcpy(response.message, userData.username);
    send(sender, &response, sizeof(response), 0);
    printf("LOG> logged in: %s\n", userData.username);
}

void processCreate(int sender, Packet packet) {
    User *currentUser = findUser(packet.username);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(users[i].room, packet.message) == 0) {
            ResponsePacket response;
            response.response_type = CREATE_FAILURE;
            strcpy(response.message, "Room already exists");
            send(sender, &response, sizeof(response), 0);
            return;
        }
    }
    sendRoomChange(*currentUser, ROOM_LEAVE);
    strcpy(currentUser->room, packet.message);
    ResponsePacket response;
    response.response_type = CREATE_SUCCESS;
    strcpy(response.message, currentUser->room);
    send(sender, &response, sizeof(response), 0);
    sendRoomChange(*currentUser, ROOM_JOIN);
    printf("LOG> %s created room: %s\n", currentUser->username, currentUser->room);
}

void processEnter(int sender, Packet packet) {
    User *currentUser = findUser(packet.username);
    char foundRoom[MAX_ROOM_NAME_SIZE] = {0};
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(users[i].room, packet.message) == 0) {
            strcpy(foundRoom, users[i].room);
            break;
        }
    }
    if (foundRoom[0] == '\0') {
        ResponsePacket response;
        response.response_type = CREATE_FAILURE;
        strcpy(response.message, "Room does not exist");
        send(sender, &response, sizeof(response), 0);
        return;
    }
    sendRoomChange(*currentUser, ROOM_LEAVE);
    strcpy(currentUser->room, packet.message);
    ResponsePacket response;
    response.response_type = CREATE_SUCCESS;
    strcpy(response.message, currentUser->room);
    send(sender, &response, sizeof(response), 0);
    sendRoomChange(*currentUser, ROOM_JOIN);
    printf("LOG> %s entered room: %s\n", currentUser->username, currentUser->room);
}

void processWho(int sender, Packet packer) {
    ResponsePacket response;
    response.response_type = WHO_RESPONSE;
    int userCount = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].username[0] != '\0') {
            strcpy(response.username, users[i].username);
            strcpy(response.message, users[i].room[0] == '\0' ? "Not In Room" : users[i].room);
            send(sender, &response, sizeof(response), 0);
            userCount++;
        }
    }
    if (userCount == 0) {
        strcpy(response.username, "Server");
        strcpy(response.message, "No users online");
        send(sender, &response, sizeof(response), 0);
    }
}

void processMessage(int sender, Packet packet) {
    User *currentUser = findUser(packet.username);
    if (currentUser->username[0] == '\0') {
        ResponsePacket response;
        response.response_type = LOGIN_FAILURE;
        strcpy(response.message, "You must login first");
        send(sender, &response, sizeof(response), 0);
        return;
    }
    if (currentUser->room[0] == '\0') {
        ResponsePacket response;
        response.response_type = CREATE_FAILURE;
        strcpy(response.message, "You must create or join a room first");
        send(sender, &response, sizeof(response), 0);
        return;
    }

    printf("LOG> %s in room %s: %s\n", packet.username, currentUser->room, packet.message);
    // Send message to all clients in the room
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (users[i].client_fd != sender && strcmp(users[i].room, currentUser->room) == 0) {
            ResponsePacket response;
            response.response_type = MESSAGE_SUCCESS;
            strcpy(response.username, packet.username);
            strcpy(response.message, packet.message);
            send(users[i].client_fd, &response, sizeof(response), 0);
        }
    }
}