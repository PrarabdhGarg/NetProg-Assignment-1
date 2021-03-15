#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_ARGS 50

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
            if(dup2(fd, 0) < 0) {
                printf("Error in duplicating file descriptor\n");
                perror("dup2");
                return;
            }  
        } else if(arg[0] == '>') {
            if(strlen(arg) > 1 && arg[1] == '>') {
                char *filename = strlen(arg) == 2 ? strtok(NULL, delim) : arg + 2;
                int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
                if(dup2(fd, 2) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
            } else {
                char *filename = strlen(arg) == 1 ? strtok(NULL, delim) : arg + 1;
                int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
                if(dup2(fd, 1) < 0) {
                    printf("Error in duplicating file descriptor\n");
                    perror("dup2");
                    return;
                }
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
    if(fork() == 0) {
        execv(commandPath, args);
        printf("Call to execv system call failed\n");
    } else {
        wait(NULL);
    }
}

void main() {
    char *input = (char *) malloc(sizeof(char) * 500);
    fgets(input, 500, stdin);
    sh_execute(input);
}