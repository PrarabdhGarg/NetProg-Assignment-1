#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLIENT_PATH_PREFIX "./init"
#define MSG_SIZE 1000
#define MAXUSR 1000
#define MAXGRP 1000
#define MAXPENDING 100

typedef struct msg{
    long messageType;
    int action;
    long messageDest;
    char message[MSG_SIZE];
    time_t timeout;
}MESSAGE;

typedef struct list{
    MESSAGE msg[MAXPENDING];
    int tail;
}LIST;

typedef struct grp{
    int gid;
    int noMembers;
    long members[MAXUSR];
    LIST pendingMsg;
}GROUP;

typedef struct state{
    int noUsers;
    int noGrps;
    long activeUsr[MAXUSR];
    GROUP groups[MAXGRP];
}STATE;
STATE state;

void init(){
    state.noGrps = 0;
    state.noUsers = 0;
    memset(state.activeUsr , 0 , sizeof(state.activeUsr));
    memset(state.groups , 0 , sizeof(state.groups));
}

int main(int argc , char* argv[]){
    key_t key = ftok(CLIENT_PATH_PREFIX , 1);
    int servMsgId = msgget(key, IPC_CREAT | 0666);

    init();

    while(1){
        MESSAGE request;
        msgrcv(servMsgId , &request , sizeof(request) , 0 , 0);

        int clientKey = ftok(CLIENT_PATH_PREFIX , request.messageType);
        int clientMsgId = msgget(clientKey, IPC_CREAT | 0666);

        if(request.action == 0){
            int flag = 0;
            int firstempty = -1;
            for(int i=0; i<MAXUSR; i++){
                if(state.activeUsr[i] == 0 && firstempty == -1)
                    firstempty = i;

                if(state.activeUsr[i] == request.messageType)
                    flag = 1;
            }
            if(flag == 1)
                continue;
            
            state.noUsers++;
            state.activeUsr[firstempty] = request.messageType;
            printf("Logged in user with id: %ld\n" , request.messageType);
        }
        else if(request.action == 1){
            int flag = 0;
            for(int i=0; i<MAXUSR; i++){
                if(state.activeUsr[i] == request.messageType){
                    state.noUsers--;
                    state.activeUsr[i] = 0;
                }
            }

            printf("Logged out user with id: %ld\n" , request.messageType);
        }
        else if(request.action == 2){
            MESSAGE response;
            response.messageType = 1;
            response.action = 2;
            
            int size = 0;
            response.message[0] = '\0';
            char* temp = response.message;

            for(int i=0; i<state.noGrps; i++){
                size += sprintf(temp+size , "%d " , state.groups[i].gid);
            }
            msgsnd(clientMsgId , &response , sizeof(response) , 0);
            printf("Group List requested by user with id: %ld\n" , request.messageType);
        }
        else if(request.action == 3){
            int newGid = 2000 + state.noGrps;

            state.groups[state.noGrps].gid = newGid;
            state.groups[state.noGrps].noMembers = 1;
            state.groups[state.noGrps].members[0] = request.messageType;
            state.groups[state.noGrps].pendingMsg.tail = 0;

            state.noGrps++;

            MESSAGE response;
            response.messageType = 1;
            response.action = 3;
            memset(response.message , 0 , sizeof(response.message));
            sprintf(response.message , "%d" , newGid);  

            msgsnd(clientMsgId , &response , sizeof(response) , 0);
            
            printf("Created Group %d for user with id %ld\n" , newGid , request.messageType);             
        }
        else if(request.action == 4){
            int Gid = atoi(request.message);
            time_t currTime = time(NULL);
            if(Gid < 2000 + state.noGrps){
                int idx = Gid - 2000;

                int flag=0;
                for(int i=0; i<state.noGrps; i++){
                    if(state.groups[idx].members[i] == request.messageType)
                        flag = 1;
                }

                if(flag == 0){
                    state.groups[idx].members[state.groups[idx].noMembers] = request.messageType;
                    state.groups[idx].noMembers++;

                    for(int j = 0; j<state.groups[idx].pendingMsg.tail; j++){
                        if(currTime > state.groups[idx].pendingMsg.msg[j].timeout){
                            for(int k = j+1; k<state.groups[idx].pendingMsg.tail; k++){
                                state.groups[idx].pendingMsg.msg[k-1] = state.groups[idx].pendingMsg.msg[k];
                            }
                            state.groups[idx].pendingMsg.tail--;
                            j--;
                        }
                        else{
                            msgsnd(clientMsgId , &state.groups[idx].pendingMsg.msg[j] , sizeof(state.groups[idx].pendingMsg.msg[j]) , 0);
                        }
                    }

                    printf("Added user with id %ld to group %d\n" , request.messageType , Gid);
                }
            }
        }
        else if(request.action == 5){
            if(request.messageDest >= 2000){
                long Gid = request.messageDest;
                request.messageDest = request.messageType;
                request.messageType = Gid;

                long idx = Gid - 2000;
                if(idx < state.noGrps){
                    state.groups[idx].pendingMsg.msg[state.groups[idx].pendingMsg.tail] = request;
                    state.groups[idx].pendingMsg.tail++;
                    for(int i=0; i<state.groups[idx].noMembers; i++){
                        int destKey = ftok(CLIENT_PATH_PREFIX , state.groups[idx].members[i]);
                        int destMsgId = msgget(destKey, IPC_CREAT | 0666);
                        printf("Sent group message of group %ld to %ld\n" , Gid , state.groups[idx].members[i]);
                        msgsnd(destMsgId , &request , sizeof(request) , 0);
                    }
                }
            }else{
                int destKey = ftok(CLIENT_PATH_PREFIX , request.messageDest);
                int destMsgId = msgget(destKey, IPC_CREAT | 0666);

                msgsnd(destMsgId , &request , sizeof(request) , 0);
                printf("Sent message from %ld to %ld\n" , request.messageType , request.messageDest);
            }
        }

    }
}