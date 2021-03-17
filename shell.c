#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<unistd.h>
#include<signal.h>
#include<sys/wait.h>
#include <setjmp.h>

#define MAX_ARGS 50
#define MAX_SHORTCUT_COMMANDS 50

char input[1000]= "";
jmp_buf return_here;

typedef struct node{
    int avail;
    int pfds[3];
    struct node* next;
}PIPE;

typedef struct {
    int index;
    char command[500];
} ShortcutCommand;

ShortcutCommand *scCommands;
int scCommandsLength = 0;

int searchInDirectory(char *directory, char *filename) {
    DIR *dir = opendir(directory);
    struct dirent* file;
    while((file = readdir(dir)) != NULL) {
        if(strcmp(file -> d_name, filename) == 0) {
            return 1;
        }
    }
    return 0;
}

char *searchInPathVariable(char *filename) {
    char *path = getenv("PATH");
    char *tok = strtok(path, ":");
    while(tok != NULL) {
        if(searchInDirectory(tok, filename) == 1) {
            strcat(tok, "/");
            strcat(tok, filename);
            return tok;
        }
        tok = strtok(NULL, ":");
    }
    return NULL;
}

void readScCommands() {
    scCommands = (ShortcutCommand *) malloc(sizeof(ShortcutCommand) * MAX_SHORTCUT_COMMANDS);
    FILE *fp = fopen(".sc_config.txt", "r");
    if(fp == NULL) {
        return;
    }
    char *input = (char *) malloc(sizeof(char) * 1000);
    while(fgets(input, 1000, fp) != NULL) {
        int len = strlen(input);
        input[len - 1] == '\n' ? input[len - 1] = '\0' : 1;
        scCommands[scCommandsLength].index = atoi(strtok(input, " "));
        strcpy(scCommands[scCommandsLength].command, strtok(NULL, "\0"));
        scCommandsLength++;
    }
}

int findScCommand(int index) {
    for(int i = 0; i < scCommandsLength; i++) {
        if(scCommands[i].index == index){
            return i;
        }
    }
    return -1;
}

void sh_execute(char *input) {
    PIPE* list = NULL;
    char *args[50];
    char *delim = " \t\n";
    char *command = strtok(input, delim);
    char *commandPath;
    args[0] = command;
    int i = 1;
    char *arg = strtok(NULL, delim);
    while(arg != NULL) {
        if(arg[0] == '<') {
            char *filename = strlen(arg) == 1 ? strtok(NULL, delim) : arg + 1;
            int fd = open(filename, O_RDONLY);
            if(fd < 0) {
                printf("Error. File doesn't exist\n");
                return;
            }
            //printf("fd of file to read = %d\n", fd);
            if(dup2(fd, 0) < 0) {
                printf("Error in duplicating file descriptor\n");
                perror("dup2");
                return;
            }
            //printf("Closed fd for stdin\n");
            //printf("Duplicated fd %d to fd 0", fd);  
        } else if(arg[0] == '>') {
            if(strlen(arg) > 1 && arg[1] == '>') {
                char *filename = strlen(arg) == 2 ? strtok(NULL, delim) : arg + 2;
                int fd = open(filename, O_RDWR | O_APPEND);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_APPEND);
                }
                //printf("fd of file to append = %d\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
                //printf("Closed fd for stdout\n");
                //printf("Duplicated fd %d to fd 1", fd);
            } else {
                char *filename = strlen(arg) == 1 ? strtok(NULL, delim) : arg + 1;
                int fd = open(filename, O_RDWR | O_TRUNC);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_TRUNC);
                }
                //printf("fd of file to write = %d\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
                //printf("Closed fd for stdout\n");
                //printf("Duplicated fd %d to fd 1", fd);
            }
        } else if(strcmp(arg , "|") == 0){
            int pfd[2];
            pipe(pfd);

            pid_t pid = fork();

            if(pid == 0){
                close(pfd[0]);
                dup2(pfd[1] , 1);

                args[i] = NULL;
                if((commandPath = searchInPathVariable(args[0])) == NULL) {
                    printf("Invalid Command\n");
                    return;
                }
                execv(commandPath, args);
                printf("Call to execv system call failed\n");

            }
            else{
                close(pfd[1]);
                dup2(pfd[0] , 0);

                int status;
                wait(&status);

                i = 0;
            }
        }else if(strcmp(arg , "||") == 0 || strcmp(arg , "|||") == 0){
            PIPE* temp = malloc(sizeof(PIPE));
            temp->next = NULL;
            if(list == NULL)
                list = temp;
            else{
                temp->next = list;
                list = temp;
            }

            if(strcmp(arg , "||") == 0)
                temp->avail = 2;
            else
                temp->avail = 3;
            int pfds[temp->avail][2];
            for(int j=0; j<temp->avail; j++){
                pipe(pfds[j]);
                temp->pfds[j] = pfds[j][0];
            }

            pid_t pid = fork();
            if(pid == 0){
                int pfd[2];

                for(int j=0; j<temp->avail; j++){
                    close(pfds[j][0]);
                }

                pipe(pfd);

                if(fork() == 0){
                    close(pfd[0]);
                    dup2(pfd[1] , 1);

                    args[i] = NULL;
                    if((commandPath = searchInPathVariable(args[0])) == NULL) {
                        printf("Invalid Command\n");
                        return;
                    }
                    execv(commandPath, args);
                    printf("Call to execv system call failed\n");
                }else{
                    close(pfd[1]);

                    int status;
                    wait(&status);

                    i = 0;
                    char buf;
                    while(read(pfd[0], &buf, 1) > 0){
                        for(int j=0; j<temp->avail; j++){
                            write(pfds[j][1], &buf, 1);
                        }
                    }
                    exit(0);
                }
            }else{
                for(int j=0; j<temp->avail; j++){
                    close(pfds[j][1]);
                }

                temp->avail--;
                dup2(temp->pfds[temp->avail] , 0);

                int status;
                wait(&status);

                i = 0;
            }

        }else if(arg[0] == ','){
            if(list == NULL){
                printf("Invalid Syntax\n");
                return;
            }

            pid_t pid = fork();

            if(pid == 0){
                args[i] = NULL;
                if((commandPath = searchInPathVariable(args[0])) == NULL) {
                    printf("Invalid Command\n");
                    return;
                }
                execv(commandPath, args);
                printf("Call to execv system call failed\n");

            }
            else{
                int status;
                wait(&status);

                list->avail--;
                dup2(list->pfds[list->avail] , 0);

                if(list->avail == 0){
                    PIPE* temp = list;
                    list = list->next;
                    free(temp);
                }

                i = 0;
            }
            
        }else {
            args[i++] = arg;
        }
        arg = strtok(NULL, delim);
    }
    args[i] = NULL;
    if((commandPath = searchInPathVariable(args[0])) == NULL) {
        printf("Invalid Command : %s\n" , args[0]);
        return;
    }
    execv(commandPath, args);
    printf("Call to execv system call failed\n");
}

void printhead()
{
    char hostname[1024] , cdir[1024];
    int hname = gethostname(hostname , 1024);
    getcwd(cdir , 1024);

    fprintf(stdout , "\033[1;33m%s@%s:\033[1;34m%s\033[0m$ " , getlogin() , hostname , cdir);
    fflush(NULL);

    return;
}

void suspend_handler(){
    exit(0);
}

void inturruptHandler(int signo) {
    printf("\nShortcut Mode:");
    int inputIndex, arrIndex;
    char ip[20];
    fgets(ip, 20, stdin);
    int len = strlen(ip);
    ip[len - 1] = '\0';
    inputIndex = atoi(ip);
    if((arrIndex = findScCommand(inputIndex)) == -1) {
        printf("Invalid Shortcut Index\n");
        input[0] = '\0';
        siglongjmp(return_here, 1);
    }
    strcpy(input , scCommands[arrIndex].command);

    siglongjmp (return_here, 1);
}

void main() {
    int shell_pgid = getpid ();
    setpgid(shell_pgid, shell_pgid);

    tcsetpgrp(STDIN_FILENO , shell_pgid);

    sigset_t block, sigint;
	sigemptyset(&block);
    sigemptyset(&sigint);
    sigaddset(&sigint, SIGINT);
	sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTTOU);
	sigaddset(&block, SIGTSTP);
	sigaddset(&block, SIGKILL);
    sigaddset(&block, SIGQUIT);

    sigprocmask(SIG_BLOCK , &block , NULL);

    readScCommands();

    while(1)
    {
        printhead();
        
        sigprocmask(SIG_UNBLOCK, &sigint, NULL);
        signal(SIGINT, inturruptHandler);

        if(sigsetjmp (return_here , 1) == 0)
            fgets(input , 1000 , stdin);

        sigprocmask(SIG_BLOCK, &sigint, NULL);
        int size = strlen(input);
        
        if(input[size - 1] == '\n')
            input[--size] = '\0';
        if(size == 0)
            continue;

        pid_t pid = fork();

        int isbackground = 0;
        if(input[size-1] == '&'){
            isbackground = 1;
            input[--size] = '\0';
        }

        if(pid == 0){
            pid = getpid();
            setpgid(pid, pid);
            if(isbackground == 0){
                tcsetpgrp(STDIN_FILENO , pid);
                sigprocmask(SIG_UNBLOCK , &block , NULL);
                signal(SIGTSTP , suspend_handler);
            }        

            sh_execute(input); 
            exit(0);       
        }
        else{      
            int status;
            if(isbackground == 0){
                waitpid(pid , &status , 0);
            }
            tcsetpgrp (STDIN_FILENO , shell_pgid);
        }
    }
}