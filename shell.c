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
            if(dup2(fd, 0) < 0) {
                printf("Error in duplicating file descriptor\n");
                perror("dup2");
                return;
            }  
        } else if(arg[0] == '>') {
            if(strlen(arg) > 1 && arg[1] == '>') {
                char *filename = strlen(arg) == 2 ? strtok(NULL, delim) : arg + 2;
                int fd = open(filename, O_RDWR | O_TRUNC);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_TRUNC);
                }

                if(dup2(fd, 2) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
            } else {
                char *filename = strlen(arg) == 1 ? strtok(NULL, delim) : arg + 1;
                int fd = open(filename, O_RDWR | O_TRUNC);
                if(fd < 0) {
                    fd = creat(filename, 0777);
                    int fd = open(filename, O_RDWR | O_TRUNC);
                }
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
            }
        } else if(arg[0] == '|'){
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

void main() {
    int shell_pgid = getpid ();
    setpgid(shell_pgid, shell_pgid);

    tcsetpgrp(STDIN_FILENO , shell_pgid);

    sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTTOU);
	sigaddset(&block, SIGTSTP);
	sigaddset(&block, SIGKILL);
    sigaddset(&block, SIGQUIT);

    sigprocmask(SIG_BLOCK , &block , NULL);

    char input[1000];

    while(1)
    {
        printhead();

        fgets(input , 1000 , stdin);
        int size = strlen(input);
        
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

