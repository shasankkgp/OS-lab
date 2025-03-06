#include<stdio.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<unistd.h>
#include<time.h>
#include<sys/shm.h>
#include<sys/msg.h>

#define MAX_COOKS 2
#define MAX_WAITERS 5
#define MAX_CUSTOMERS 200

#define key 1234

#define W_1 100
#define W_2 300
#define W_3 500
#define W_4 700
#define W_5 900
#define C_1 1100
#define MUTEX 13

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

typedef struct customer{
    int id;
    int arrival_time;
    int count;
} customer;

int *M;
int mutexid, cookid, waiterid, customerid;
struct sembuf pop, vop;

void print_time(){
    printf("[%02d:%02d am]",11+M[0]/60,M[0]%60);
}

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void cmain(int id, int arrival_time, int count) {
    // code for customers 
    if (M[0] > 240) return;
    if (M[1] == 0) return;
    while (1) {

        print_time();
        printf("Customer %d arrives (count = %d)\n", id, count);

        if( M[1] == 0 ){
            print_time();
            printf("Customer %d leaves (no empty table)\n", id);
            return;
        }

        P(mutexid);
        print_time();
        printf("Got the mutex lock\n");

        int current_time = M[0];

        M[1]--;  // decrement in empty tables
        int back = M[W_1+200*((id-1)%5)+3];  // read the back of the queue
        M[back]= id;  // store customer id in waiter's location
        M[back+1] = count;  // store count in waiter's location
        M[W_1+200*((id-1)%5)+3] = (back+2)%2000;  // update back of the queue
        M[W_1+200*((id-1)%5)+1]++;  // increase the waiting orders for the waiter

        printf("Released the mutex lock\n");
        V(mutexid);
        

        pop.sem_num = (id-1)%5;  // specify which waiter to wake up
        printf("Signalling waiter %c\n", 'U' + (id-1)%5);
        V(waiterid);   // signal to the waiter to take order

        // place order statement
        print_time();
        printf("Customer %d: Order placed to Waiter %c\n", id, 'U' + (id-1)%5);

        pop.sem_num = id;  // specify which customer to wake up
        P(customerid);  // wait for order to come 

        // eat food, eating takes 30 min time 
        int time_to_sleep = 30 * 100;  // 30 min time
        usleep(time_to_sleep);
        P(mutexid);

        // eat food statement
        print_time();
        printf("Customer %d inishes eating and leaves\n", id);

        M[0]= current_time + 30;  // update the current time
        M[1]++;   // empty table is increased by 1 
        V(mutexid);
    }
}

int main() {

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    int shmid = shmget(key, 2000 * sizeof(int), 0777 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    M = (int*)shmat(shmid, (void*)0, 0);
    if (M == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }

    mutexid = M[MUTEX];
    cookid = M[MUTEX + 1];
    waiterid = M[MUTEX + 2];
    customerid = M[MUTEX + 3];

    FILE *f = fopen("customers.txt", "r");
    if (f == NULL) {
        perror("fopen failed");
        exit(1);
    }

    int last_arrival_time = 0;
    int id, arrival_time, count;

    while (fscanf(f, "%d %d %d", &id, &arrival_time, &count) == 3) {
        if (arrival_time < last_arrival_time) {
            fprintf(stderr, "Error: Customer %d has an invalid arrival time.\n", id);
            continue;
        }

        int delay = (arrival_time - last_arrival_time) * 100; // Convert minutes to simulated time
        usleep(delay);
        P(mutexid);
        M[0] = arrival_time;
        V(mutexid);
        last_arrival_time = arrival_time;

        if (fork() == 0) {
            cmain(id, arrival_time, count);
            exit(0);
        }
    }

    fclose(f);

    while (wait(NULL) > 0);  // Wait for all customers to finish

    // Clean up shared memory and semaphores
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(mutexid, 0, IPC_RMID);
    semctl(cookid, 0, IPC_RMID);
    semctl(waiterid, 0, IPC_RMID);
    semctl(customerid, 0, IPC_RMID);

    return 0;
}
