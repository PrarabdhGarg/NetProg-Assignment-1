#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>

#define CLIENT_PATH_PREFIX "./init"
#define MAX_MSG_LEN 1000

typedef struct {
    long messageType;    // userid of client
    int action;         // 0 -> Login   1 -> Logout   2 -> List Groups   3 -> Create Group   4 -> Join Group 
    long dest;
    char message[MAX_MSG_LEN];      //  0 -> username 1 -> username 2 -> empty        3 -> Empty    4 -> Group Id
    time_t timeout;
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
        printf("\033[1;32mEnter 1 to logout, 2 to list groups, 3 to create group, 4 to join group, 5 to send message, 6 to read new messages\n\033[0m");
        int choice;
        scanf("%d", &choice);
        if(choice <= 0 || choice > 6) {
            printf("Invalid Choice\n");
            continue;
        }
        ServerMessage message;
        message.action = choice;
        message.messageType = userId;
        message.dest = 1;
        message.timeout = 60;
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
            printf("\033[1;34mList of available group ids = \n\033[0m");
            char *groupId = strtok(response.message, " ");
            while(groupId != NULL) {
                printf("\t%s\n", groupId);
                groupId = strtok(NULL, " ");
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
            printf("\033[1;34mGroup id of newly created group = \033[0m%s\n", response.message);
        } else if(choice == 4) {
            printf("\033[1;32mEnter groupid of the group you wish to join\n\033[0m");
            scanf("%s", message.message);
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Failed to Get groups");
                continue;
            }
            printf("\033[1;34mJoined group sucessfully\n\033[0m");
        } else if(choice == 5) {
            printf("\033[1;32mEnter Group id or username to send the message\n\033[0m");
            char name[100];
            scanf("%s", name);
            
            if((message.dest = atoi(name)) == 0) {
                message.dest = hash(name);
            } else {
                printf("\033[1;32mSet message timeout. Default timeout 60 seconds\n\033[0m");
                int x;
                scanf("%d", &x);
                message.timeout = time(NULL) + x;
            }
            printf("\033[1;32mEnter message to send\n\033[0m");
            char buff[1000];
            getchar();
            fgets(buff , 1000 , stdin);
            
            sprintf(message.message , "\033[1;34m%s: \033[0m%s" , argv[1] , buff);
            if(msgsnd(serverMsgqId, &message, sizeof(message), 0) < 0) {
                perror("Failed to send message");
                continue;
            }
        } else if(choice == 6) {
            printf("\033[1;32mEnter group id or username to read new messages\n\033[0m");
            char name[100];
            scanf("%s", name);
            int dest;
            if((dest = atoi(name)) == 0) {
                dest = hash(name);
            }
            printf("\033[1;32mNew Messages: \n\033[0m");
            ServerMessage tempMessage;
            while(msgrcv(clientMsgqId, &tempMessage, sizeof(tempMessage), dest, IPC_NOWAIT) != -1) {
                printf("%s\n", tempMessage.message);
            }
        }
    }
}