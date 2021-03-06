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
char *savePtr;

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
    scCommandsLength = 0;
    while(fgets(input, 1000, fp) != NULL) {
        int len = strlen(input);
        input[len - 1] == '\n' ? input[len - 1] = '\0' : 1;
        scCommands[scCommandsLength].index = atoi(strtok(input, " "));
        strcpy(scCommands[scCommandsLength].command, strtok(NULL, "\0"));
        scCommandsLength++;
    }
    fclose(fp);
}

int writeScCommand(int index, char *command) {
    FILE *fp = fopen(".sc_config.txt", "a");
    if(fp == NULL)
        return -2;
    scCommands[scCommandsLength].index = index;
    strcpy(scCommands[scCommandsLength].command, command);
    scCommandsLength++;
    fprintf(fp, "%d %s\n", index, command);
    fflush(fp);
    fclose(fp);
    return 0;
}

int removeScCommand(int index) {
    FILE *fp = fopen(".sc_config.txt", "w");
    int ret = -1;
    if(fp == NULL)
        return -1;
    for(int i = 0; i < scCommandsLength; i++) {
        if(index != scCommands[i].index) {
            fprintf(fp, "%d %s\n", scCommands[i].index, scCommands[i].command);  
            ret++;
        }     
    }
    fclose(fp);
    if(ret == 0)
        readScCommands();
    return ret;
}

int findScCommand(int index) {
    printf("Commands = %d\n", scCommandsLength);
    for(int i = 0; i < scCommandsLength; i++) {
        if(scCommands[i].index == index){
            return i;
        }
    }
    return -1;
}

void sh_execute(char *input) {
    int saved_ip = dup(0);
    int saved_op = dup(1);

    FILE* ip_stream = fdopen(saved_ip , "r");
    FILE* op_stream = fdopen(saved_op , "w");

    PIPE* list = NULL;
    char *args[50];
    char *delim = " \t\n";
    char *command = strtok_r(input, delim, &savePtr);
    if(strcmp(command, "sc") == 0) {
        char *arg = strtok_r(NULL, delim, &savePtr);
        if(strcmp(arg, "-d") == 0) {
            int index = atoi(strtok_r(NULL, delim, &savePtr));
            if(removeScCommand(index) == -1) {
                printf("Error removing shortcut command\n");
                return;
            }
            printf("Shortcut command removed sucessfully\n");
            return;
        } else if(strcmp(arg, "-i") == 0) {
            char *temp = strtok_r(NULL, delim, &savePtr);
            int index = atoi(temp);
            char *comm = strtok_r(NULL, "\0", &savePtr);
            if(writeScCommand(index, comm) == -1) {
                printf("Error while adding Shortcut command\n");
                return;
            }
            printf("Shortcut command added sucessfully\n");
            return;
        } else {
            printf("Invlaid Argument\n");
            return;
        }
    }
    char *commandPath;
    args[0] = command;
    int i = 1;
    char *arg;
    if(savePtr != NULL && savePtr[0] == '"') {
        arg = strtok_r(NULL, "\"", &savePtr);
    } else {
        arg = strtok_r(NULL, delim, &savePtr);
    }
    while(arg != NULL) {
        if(arg[0] == '<') {
            char *filename = strlen(arg) == 1 ? strtok_r(NULL, delim, &savePtr) : arg + 1;
            int fd = open(filename, O_RDONLY);
            if(fd < 0) {
                printf("Error. File doesn't exist\n");
                return;
            }
            fprintf(op_stream , "Input Redirection Original Fd = %d, Remapped Fd = 0\n", fd);
            if(dup2(fd, 0) < 0) {
                printf("Error in duplicating file descriptor\n");
                perror("dup2");
                return;
            }
        } else if(arg[0] == '>') {
            if(strlen(arg) > 1 && arg[1] == '>') {
                char *filename = strlen(arg) == 2 ? strtok_r(NULL, delim, &savePtr) : arg + 2;
                int fd = open(filename, O_RDWR | O_APPEND);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_APPEND);
                }
                fprintf(op_stream , "Output Redirection Fd = %d, Remapped Fd = 1\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
            } else {
                char *filename = strlen(arg) == 1 ? strtok_r(NULL, delim, &savePtr) : arg + 1;
                int fd = open(filename, O_RDWR | O_TRUNC);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_TRUNC);
                }
                fprintf(op_stream , "Output Redirection Fd = %d, Remapped Fd = 1\n", fd);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
            }
        } else if(strcmp(arg , "|") == 0){
            int pfd[2];
            pipe(pfd);

            fprintf(op_stream , "Pipe Input Fd = %d, Pipe Output Fd = %d\n" , pfd[0] , pfd[1]);

            pid_t pid = fork();

            if(pid == 0){
                close(pfd[0]);
                dup2(pfd[1] , 1);

                args[i] = NULL;
                fprintf(op_stream , "Pipe Step: %s, Process Id: %d\n", args[0] , getpid());
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

                fprintf(op_stream , "Pipe Input Fd: %d , Pipe Output Fd: " , pfd[1]);

                for(int j=0; j<temp->avail; j++){
                    fprintf(op_stream , "%d,", pfds[j][0]);
                    close(pfds[j][0]);
                }

                fprintf(op_stream , "\n");

                pipe(pfd);

                if(fork() == 0){
                    close(pfd[0]);
                    dup2(pfd[1] , 1);

                    fprintf(op_stream , "Pipe Step: %s, Process Id: %d\n", args[0] , getpid());

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

        }else if(arg[strlen(arg)- 1] == ','){
            
            if(strlen(arg) > 1){
                arg[strlen(arg)- 1] = '\0';
                args[i++] = arg;
            }

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

                fprintf(op_stream , "Pipe Step: %s, Process Id: %d\n", args[0] , getpid());

                execv(commandPath, args);
                printf("Call to execv system call failed\n");

            }
            else{
                int status;
                wait(&status);

                list->avail--;
                dup2(list->pfds[list->avail] , 0);
                dup2(saved_op , 1);

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
        if(savePtr != NULL && savePtr[0] == '"') {
            arg = strtok_r(NULL, "\"", &savePtr);
        } else {
            arg = strtok_r(NULL, delim, &savePtr);
        }
    }
    args[i] = NULL;
    fprintf(op_stream , "Final Step: %s, Process Id: %d\n", args[0] , getpid());
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

    while(1)
    {
        printhead();
        readScCommands();
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
                if(WIFEXITED(status)){
                    WEXITSTATUS(status);
                    printf("Process Id:%d Exited with Status:%d\n", pid, status);
                }
                else if(WIFSTOPPED(status)){
                    WSTOPSIG(status);
                    printf("Process Id:%d Stopped with Status:%d\n", pid, status);
                }
                else if(WIFSIGNALED(status)){
                    WTERMSIG(status);
                    printf("Process Id:%d Terminated with Status:%d\n", pid, status);
                }
                
            }
            tcsetpgrp (STDIN_FILENO , shell_pgid);
        }
    }
}