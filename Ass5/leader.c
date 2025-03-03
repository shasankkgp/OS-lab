#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : 10;
    srand((unsigned int)time(NULL));
    key_t key = ftok("/", 4);
    int shmid = shmget(key, (104) * sizeof(int), 0777 | IPC_CREAT);
    printf("shmid = %d\n", shmid);
    fflush(stdout);
    if (shmid == -1) {
        perror("Shared memory creation failed");
        exit(1);
    }
    int *M = (int *)shmat(shmid, 0, 0);
    if (M == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }
    M[0] = n;
    M[1] = 0;
    M[2] = 0;
    int flag=0;
    for (int i = 0; i <= n; i++) {
        M[i + 3] = 0;
    }
    int hash[1001] = {0};
    int sum = 0;

    while (M[1] != M[0] ) {} // busy wait

    while (1) {
        while (M[2] != 0) {} // busy wait
        if(flag==1){
            sum = M[3];
            printf("%d + ", M[3]);
            for (int i = 0; i < n; i++) {
                sum += M[i + 4];
                printf("%d + ", M[i + 4]);
            }
            printf(" = %d\n", sum);
            hash[sum]++;
            if (hash[sum] > 1) {
                M[2] = -1;
                printf("sum = %d is repeated\n", sum);
                while (M[2] != 0) {} // busy wait
                break;
            }
        }
        // fflush(stdout);
        M[3] = 1 + rand() % 99;
        M[2] += 1;
        flag = 1;
        // fflush(stdout);
    }
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}