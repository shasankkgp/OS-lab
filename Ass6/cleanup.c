#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define KEY 1234  // Same key as in your main program

int main() {
    int shmid = shmget(KEY, 0, 0666);  // Get shared memory ID
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL); // Remove shared memory
        printf("Shared memory removed.\n");
    } else {
        perror("shmget");
    }

    key_t sem_key = ftok("cook.c", 'S'); // Generate unique key (same as in your program)
    int mutexid = semget(sem_key, 1, 0666);
    int cookid = semget(sem_key + 1, 1, 0666);
    int waiterid = semget(sem_key + 2, 5, 0666);
    int customerid = semget(sem_key + 3, 200, 0666);
    
    if (mutexid != -1) {
        semctl(mutexid, 0, IPC_RMID);
        printf("Mutex semaphore removed.\n");
    }
    if (cookid != -1) {
        semctl(cookid, 0, IPC_RMID);
        printf("Cook semaphore removed.\n");
    }
    if (waiterid != -1) {
        semctl(waiterid, 0, IPC_RMID);
        printf("Waiter semaphore set removed.\n");
    }
    if (customerid != -1) {
        semctl(customerid, 0, IPC_RMID);
        printf("Customer semaphore set removed.\n");
    }
    
    return 0;
}
