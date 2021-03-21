#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 8000
#define LOCAL_ADDRESS "127.0.0.1"

typedef struct {
    char name[500];
    char ipAddress[30];
} NodeAddress;

NodeAddress addresses[100];
int noOfAdresses;

void readConfigFile(char *path) {
    FILE *fp = fopen(path, "r");
    if(fp == NULL) {
        printf("Unable to read config file\n");
        exit(0);
    }
    noOfAdresses = 0;
    char name[500];
    char ip[30];
    while(fscanf(fp, "%s %s", name, ip) != EOF) {
        strcpy(addresses[noOfAdresses].name, name);
        strcpy(addresses[noOfAdresses].ipAddress, ip);
        noOfAdresses++;
    }
    return;
}

char *getIP(char *name) {
    for(int i = 0; i < noOfAdresses; i++) {
        if(strcmp(name, addresses[i].name) == 0) {
            return addresses[i].ipAddress;
        }
    }
    return NULL;
}

char *sendCommandToServer(char *ip, int port, char *command, char *input) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket()");
        return NULL;
    }

    struct sockaddr_in serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);
    if(connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect()");
    }

    int cmdlen = strlen(command);
    send(sock , &cmdlen , sizeof(int) , 0);

    send(sock , command , cmdlen+1 , 0);

    send(sock , input , strlen(input)+1 , 0);

    // char *buff = (char *)malloc(sizeof(int) + strlen(command) + strlen(input));
    // *buff = strlen(command);
    // strcpy(buff + sizeof(int), command);
    // strcpy(buff + sizeof(int) + strlen(command), input); 

    // printf("%s\n" , buff);
    // if(send(sock, buff, sizeof(int) + strlen(command) + strlen(input), 0) < 0) {
    //     perror("send()");
    // }

    shutdown(sock , SHUT_WR);

    int oplen;
    recv(sock , &oplen , sizeof(int) , 0);
    oplen++;

    char *recv_buff = (char *)malloc(sizeof(char) * oplen);
    recv(sock , recv_buff , oplen+1 , 0);
    return recv_buff;
}

char *execCommand(char *command, char *input) {
    char *tempCommand = (char *) malloc(sizeof(char) * (strlen(command) + 1));
    strcpy(tempCommand, command);
    char *temp;
    char *first = strtok_r(tempCommand, " \t\n\0", &temp);
    if(strchr(first, '.') == NULL) {
        // Send command to local-host
        return sendCommandToServer(LOCAL_ADDRESS, SERVER_PORT, command, input);
    } else {
        // Extract name
        char *name = strtok_r(first, ".", &temp);
        return sendCommandToServer(getIP(name), SERVER_PORT, command, input);
    }
}

void printhead()
{
    fprintf(stdout , "\033[1;34m%s\033[0m$ " , "ClusterShell");
    fflush(NULL);

    return;
}

void main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Incorrect number of arguments supplied. Please give the path of config file as argument\n");
        exit(0);
    }
    readConfigFile(argv[1]);
    
    char input[1000];
    char *prevOut = "";
    while(1) {
        printhead();
        fgets(input, 1000, stdin);
        int size = strlen(input);
        if(input[size - 1] == '\n')
            input[--size] = '\0';
        if(size == 0)
            continue;
        char *command = strtok(input, "|");
        int commands = 0;
        while(command != NULL) {
            prevOut = execCommand(command, prevOut);;
            commands++;
            command = strtok(NULL, "|");
        }
        printf("%s" , prevOut);
    }
}