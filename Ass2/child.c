/*
    NAME : G . SAI SHASANK 
    ROLL NO : 22CS10025
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int childPid[100];
int ind, count;
int status = 2; // 0:MISS, 1:CATCH, 2:PLAYING, 3:OUTOFGAME

void sigusr_handler(int sig) {
    if (sig == SIGUSR2) {
        int random_value = rand() % 10;
        if (random_value < 2) {
            kill(getppid(), SIGUSR2);
            status = 0;
        } else {
            kill(getppid(), SIGUSR1);
            status = 1;
        }
    } else if (sig == SIGUSR1) {
        if (status == 2) {
            printf("  . . . . ");
        } else if (status == 1) {
            status = 2;
            printf("   CATCH  ");
        } else if (status == 0) {
            status = 3;
            printf("   MISS   ");
        } else {
            printf("          ");
        }
        fflush(stdout);
        if (ind == count - 1) {
            FILE *fp = fopen("dummypid.txt", "r");
            int dummy_pid;
            fscanf(fp, "%d", &dummy_pid);
            fclose(fp);
            kill(dummy_pid, SIGINT);
        } else {
            kill(childPid[ind + 1], SIGUSR1);
        }
    }
}

void sigint_handler(int sig) {
    if (status == 1 || status == 2) {
        printf("\n+++ Child %d: Yay! I am the winner!\n", ind + 1);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGUSR1, sigusr_handler);
    signal(SIGUSR2, sigusr_handler);
    signal(SIGINT, sigint_handler);
    ind = atoi(argv[1]);
    FILE *fp = fopen("childpid.txt", "r");
    fscanf(fp, "%d", &count);
    srand(time(NULL) ^ getpid());
    for (int i = 0; i < count; i++) {
        fscanf(fp, "%d", &childPid[i]);
    }
    fclose(fp);
    while (1) {
        pause();
    }
    return 0;
}