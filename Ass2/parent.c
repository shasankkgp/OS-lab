/*
    NAME : G . SAI SHASANK 
    ROLL NO : 22CS10025
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#define num_child 10

int n, count;
int childPid[100];
int status[100];
int ind = 0;

void my_handler(int sig) {
    if (sig == SIGUSR1) {
        status[ind] = 0;
        ind = (ind + 1) % n;
    } else if (sig == SIGUSR2) {
        status[ind] = 1;
        count--;
        ind = (ind + 1) % n;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGUSR1, my_handler);
    signal(SIGUSR2, my_handler);
    n = num_child;
    if (argc == 2) {
        n = atoi(argv[1]);
    }
    count = n;
    FILE *fp = fopen("childpid.txt", "w");
    fprintf(fp, "%d\n", n);
    fclose(fp);
    printf("Parent: %d child processes created\n", n);
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid) {
            FILE *fl = fopen("childpid.txt", "a");
            fprintf(fl, "%d\n", pid);
            childPid[i] = pid;
            fclose(fl);
        } else if (pid == 0) {
            sleep(1);
            char num[10];
            sprintf(num, "%d", i);
            execlp("./child", "./child", num, NULL);
        }
    }
    printf("Parent: Waiting for child processes to read child database\n\n");
    sleep(2);
    for( int i=0 ; i<n ; i++ ){
        status[i]=0;
    }
    printf(" ");
    for (int i = 0; i < n; i++) {
        printf("     %d    ", i + 1);
    }
    printf("\n+-");
    for (int i = 0; i < n; i++) {
        printf("----------");
    }
    printf("-+\n");
    while (count > 1) {
        while (status[ind] != 0) {
            ind = (ind + 1) % n;
        }
        kill(childPid[ind], SIGUSR2);
        pause();
        int dummy_pid = fork();
        if (!dummy_pid ) {
            execlp("./dummy", "./dummy",0, NULL);
        } else {
            printf("|");
            fflush(stdout);
            FILE *fd = fopen("dummypid.txt", "w");
            fprintf(fd, "%d", dummy_pid);
            fclose(fd);
            kill(childPid[0], SIGUSR1);
            waitpid(dummy_pid, NULL, 0);
            printf("  |\n+-");
            for (int i = 0; i < n; i++) {
                printf("----------");
            }
            printf("-+\n");
        }
    }
    printf(" ");
    for (int i = 0; i < n; i++) {
        printf("     %d    ", i + 1);
    }
    fflush(stdout);
    for (int i = 0; i < n; i++) {
        kill(childPid[i], SIGINT);
    }
    exit(0);
}