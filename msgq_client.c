#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#define CLIENT_PATH_PREFIX "/init"
#define MAX_MSG_LEN 1000

typedef struct {
    long messageType;    // userid of client
    int action;         // 0 -> Login   1 -> Logout   2 -> List Groups   3 -> Create Group   4 -> Join Group 
    char message[MAX_MSG_LEN];      //  0 -> username 1 -> username 2 -> empty        3 -> Empty    4 -> Group Id
} ServerMessage;

int hash(char *username) {
    int len = strlen(username);
    int res = 0;
    for(int i = 0; i < len; i++) {
        res += username[i];
    }
    return (res % 1000) + 2;
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("The program expects username of user as an argument\n");
        exit(0);
    }
    int userId = hash(argv[1]);
    key_t serverKey = ftok(CLIENT_PATH_PREFIX, 1);
    key_t clientKey = ftok(CLIENT_PATH_PREFIX, userId);
    int serverMsgqId = msgget(serverKey, IPC_CREAT | 0666);
    int clientMsgqId = msgget(clientKey, IPC_CREAT | 0666);

    ServerMessage loginMessage;
    loginMessage.messageType = userId;
    loginMessage.action = 0;
    strcpy(loginMessage.message, argv[1]);
    if(msgsnd(serverMsgqId, &loginMessage, sizeof(loginMessage), 0) < 0) {
        perror("Login Failed");
        exit(0);
    }

    while(1) {
        printf("Enter 1 to logout, 2 to list groups, 3 to create group, 4 to join group\n");
        int choice;
        scanf("%d\n", &choice);
        if(choice <= 0 || choice > 4) {
            printf("Invalid Choice\n");
            continue;
        }
        ServerMessage message;
        message.action = choice;
        message.messageType = userId;
        if(choice == 1) {
            strcpy(message.message, argv[1]);
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Logout Failed");
                continue;
            }
            exit(0);
        } else if(choice == 2) {
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Failed to Get groups");
                continue;
            }
            int size;
            ServerMessage response;
            if((size = msgrcv(clientMsgqId, &response, sizeof(response), 1, 0)) < 0) {
                perror("Failed to get groups");
                continue;
            } 
            printf("List of available group ids = \n");
            char *groupId = strtok(response.message, " ");
            while(groupId != NULL) {
                printf("%s\n", groupId);
            }
        } else if(choice == 3) {
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Failed to Get groups");
                continue;
            }
            int size;
            ServerMessage response;
            if((size = msgrcv(clientMsgqId, &response, sizeof(response), 1, 0)) < 0) {
                perror("Failed to get groups");
                continue;
            } 
            printf("Group id of newly created group = %s\n", response.message);
        } else {
            printf("Enter groupid of the group you wish to join\n");
            scanf("%s\n", message.message);
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Failed to Get groups");
                continue;
            }
            printf("Joined group sucessfully");
        }
    }
}