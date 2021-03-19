#include<sys/socket.h>
#include<sys/types.h>
#include <netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <arpa/inet.h>

#define PORT 8000

typedef struct state{
    char ip[30];
    char cwd[1000];
}STATE;

STATE states[100];
int no_of_nodes;
FILE* config;

void readConfig(){
    config = fopen("server_config.txt" , "r");
    no_of_nodes = 0;

    while(!feof(config)){
        fscanf(config , "%s %s" , states[no_of_nodes].ip , states[no_of_nodes].cwd);
        no_of_nodes++;
    }
}

char* get_cwd(char* ip){
    for(int i=0; i<no_of_nodes; i++){
        if(strcmp(ip , states[i].ip)==0)
            return states[i].cwd;
    }

    strcpy(states[no_of_nodes].ip , ip);
    strcpy(states[no_of_nodes].cwd , getenv("HOME"));

    no_of_nodes++;

    return states[no_of_nodes-1].cwd;
}

int main(){
    int listenfd;
    if((listenfd = socket(PF_INET , SOCK_STREAM , 0)) < 0){
        printf("Socket creation failed\n");
        exit(0);
    }

    readConfig();
    chdir(getenv("HOME"));

    struct sockaddr_in servaddr;
    bzero(&servaddr , sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    bind(listenfd , (struct sockaddr*) &servaddr , sizeof(servaddr));
    listen(listenfd , 5);

    struct sockaddr_in cliaddr;

    for(; ;){
        int clilen = sizeof(cliaddr);
        int connfd = accept(listenfd , (struct sockaddr*) &cliaddr , &clilen);

        char* ip = inet_ntoa(cliaddr.sin_addr);
        char* state = get_cwd(ip);
        chdir(state);

        int cmdlen;
        recv(connfd , &cmdlen , sizeof(cmdlen) , 0);

        char cmd[cmdlen+1];
        recv(connfd , cmd , cmdlen+1 , 0);

        int ipfd[2] , opfd[2];
        pipe(ipfd);
        pipe(opfd);
        
        if(fork() == 0){
            close(opfd[0]);
            dup2(opfd[1] , 1);

            close(ipfd[1]);
            dup2(ipfd[0] , 0);

            char* args[100];
            char *delim = " \t\n";
            char* arg = strtok(cmd , delim);

            int i=0;
            while(arg != NULL){
                args[i++] = arg;
                arg = strtok(NULL , delim);
            }

            execvp(args[0] , args);
            printf("Exec call failed");
            exit(0);
        }else{
            close(opfd[1]);
            close(ipfd[0]);

            char buf;
            while(recv(connfd , &buf , 1 , 0) > 0){
                write(ipfd[1] , &buf , 1);
            }
            close(ipfd[1]);

            while(read(opfd[0] , &buf , 1) > 0){
                send(connfd , &buf , 1 , 0);
            }
            close(connfd);
            close(opfd[0]);
        }
    }
}