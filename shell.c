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

#define MAX_ARGS 50
#define MAX_SHORTCUT_COMMANDS 50

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
        if(scCommands[i].index == index)
            return i;
    }
    return -1;
}

void sh_execute(char *input) {
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
            printf("fd of file to read = %d\n", fd);
            if(dup2(fd, 0) < 0) {
                printf("Error in duplicating file descriptor\n");
                perror("dup2");
                return;
            }
            printf("Closed fd for stdin\n");
            printf("Duplicated fd %d to fd 0", fd);  
        } else if(arg[0] == '>') {
            if(strlen(arg) > 1 && arg[1] == '>') {
                char *filename = strlen(arg) == 2 ? strtok(NULL, delim) : arg + 2;
                int fd = open(filename, O_RDWR | O_APPEND);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_APPEND);
                }
                printf("fd of file to append = %d\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
                printf("Closed fd for stdout\n");
                printf("Duplicated fd %d to fd 1", fd);
            } else {
                char *filename = strlen(arg) == 1 ? strtok(NULL, delim) : arg + 1;
                int fd = open(filename, O_RDWR | O_TRUNC);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_TRUNC);
                }
                printf("fd of file to write = %d\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
                printf("Closed fd for stdout\n");
                printf("Duplicated fd %d to fd 1", fd);
            }
        } else {
            args[i++] = arg;
        }
        arg = strtok(NULL, delim);
    }
    args[i] = NULL;
    if((commandPath = searchInPathVariable(command)) == NULL) {
        printf("Invalid Command\n");
        return;
    }
    printf("Command Path = %s", commandPath);
    if(fork() == 0) {
        execv(commandPath, args);
        printf("Call to execv system call failed\n");
    } else {
        wait(NULL);
    }
}

void printhead()
{
    char hostname[1024] , cdir[1024];
    int hname = gethostname(hostname , 1024);
    getcwd(cdir , 1024);

    fprintf(stdout , "\033[1;32m%s@%s:\033[1;34m%s\033[0m$ " , getlogin() , hostname , cdir);
    fflush(NULL);

    return;
}

void suspend_handler(){
    exit(0);
}

void inturruptHandler(int signo) {
    printf("Shortcut Mode\n");
    int inputIndex, arrIndex;
    char ip[20];
    fgets(ip, 20, stdin);
    int len = strlen(ip);
    ip[len - 1] = '\0';
    inputIndex = atoi(ip);
    if((arrIndex = findScCommand(inputIndex)) == -1) {
        printf("Invalid Shortcut Index\n");
        return;
    }
    printf("Index = %d Command = %s\n", arrIndex, scCommands[arrIndex].command);
    sh_execute(scCommands[arrIndex].command);
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

    char input[1000];

    readScCommands();

    while(1)
    {
        printhead();
        
        sigprocmask(SIG_UNBLOCK, &sigint, NULL);
        signal(SIGINT, inturruptHandler);

        fgets(input , 1000 , stdin);
        sigprocmask(SIG_BLOCK, &sigint, NULL);
        int size = strlen(input);
        
        input[--size] = '\0';
        if(size == 0)
            continue;

        printf("Input = %s\n", input);

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