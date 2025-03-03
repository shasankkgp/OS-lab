#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include<sys/wait.h>
#include <unistd.h>
#include<time.h>

int main(int argc, char *argv[]) {
    int nf = (argc > 1) ? atoi(argv[1]) : 1;
    srand((unsigned int)time(NULL));
    key_t key = ftok("/", 4);
    int shmid = shmget(key, (104) * sizeof(int), 0777|IPC_EXCL);
    if (shmid == -1) {
        perror("Shared memory access failed");
        exit(1);
    }
    int *M = (int *)shmat(shmid, 0, 0);
    if (M == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    int index;
    int neg_ind;

    for (int i = 0; i < nf; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            if (M[1] < M[0]) {
                M[1]++;
                printf("Follower %d joined\n", M[1]);
                fflush(stdout);
                index = M[1];
                neg_ind = -1 * index;
            } else {
                printf("%d followers already joined\n", M[0]);
                _exit(0);
            }
            while (1) {
                while (M[2] != index && M[2] != neg_ind) {} // busy wait
                printf("follower %d is in\n", index);
                fflush(stdout);
                if (M[2] == neg_ind) {
                    if( M[2] == M[0] ){
                        M[2] = 0;
                    }else{
                        M[2] = -M[2]-1;
                    }
                    // M[1]--;
                    shmdt(M);
                    _exit(0);
                }
                M[index + 3] = 1 + rand() % 9;
                if( M[2] == M[0] ){
                    M[2] = 0;
                }else{
                    M[2] = M[2]+1;
                }
            }
        }
    }
    for (int i = 0; i < nf; i++) {
        wait(NULL);
    }
    // shmdt(M);
    return 0;
}