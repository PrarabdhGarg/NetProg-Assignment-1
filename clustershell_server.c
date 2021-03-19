#include<sys/socket.h>
#include<sys/types.h>
#include <netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define PORT 8000

typedef struct state{
    in_addr addr;
    char cwd[1000];
}STATE;

int main(){
    int listenfd;
    if((listenfd = socket(PF_INET , SOCK_STREAM , 0)) < 0){
        printf("Socket creation failed\n");
        exit(0);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr , sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

}