#ifndef LAB4_DEFINITIONS_H
#define LAB4_DEFINITIONS_H

#include <sys/socket.h>

#define PORT 12345
#define MAX_INPUT_SIZE 1024
#define MAX_MESSAGE_SIZE 1024
#define MAX_USERNAME_SIZE 64
#define MAX_ROOM_NAME_SIZE 64

#define MAX_CLIENTS 10

typedef enum {
    LOGIN,
    CREATE,
    ENTER,
    WHO,
    MESSAGE,
} RequestType;

typedef enum {
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    CREATE_SUCCESS,
    CREATE_FAILURE,
    MESSAGE_SUCCESS,
    ROOM_JOIN,
    ROOM_LEAVE,
    SHUTDOWN,
    WHO_RESPONSE
} ResponseType;

typedef struct {
    RequestType request_type;
    char username[MAX_USERNAME_SIZE];
    char message[MAX_MESSAGE_SIZE];
} Packet;

typedef struct {
    ResponseType response_type;
    char username[MAX_USERNAME_SIZE];
    char message[MAX_MESSAGE_SIZE];
} ResponsePacket;

#endif
