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
#include<signal.h>

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

#define P(s) semop(s, &pop, 1)    // wait
#define V(s) semop(s, &vop, 1)    // signal

typedef struct customer{
    int id;
    int arrival_time;
    int count;
} customer;

int *M;
int mutexid, cookid, waiterid, customerid,shmid;
struct sembuf pop, vop;

void print_time() {
    int total_minutes = 11 * 60 + M[0];  // Convert 11:00 AM base to minutes
    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;
    
    // Convert to 12-hour format
    int display_hour = (hours % 12 == 0) ? 12 : hours % 12;
    const char *period = (hours % 24 >= 12) ? "pm" : "am";
    
    printf("[%02d:%02d %s]", display_hour, minutes, period);
}

void handler(int sig) {
    // Clean up IPC resources
    semctl(mutexid, 0, IPC_RMID);
    semctl(cookid, 0, IPC_RMID);
    semctl(waiterid, 0, IPC_RMID);
    semctl(customerid, 0, IPC_RMID);
    
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);
    
    exit(0);
}

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void cmain(int id, int arrival_time, int count) {
    // code for customers 
    if (M[0] > 240){
        print_time();
        printf("Customer %d leaves (restaurant is closed)\n", id);
        return;
    }
    

    print_time();
    printf("Customer %d arrives (count = %d)\n", id, count);

    P(mutexid);
    if( M[1] == 0 ){
        V(mutexid);
        print_time();
        printf("Customer %d leaves (no empty table)\n", id);
        return;
    }

    // print_time();
    // printf("Got the mutex lock\n");

    int current_time = M[0];

    M[1]--;  // decrement in empty tables

    int waiter_index = (id-1)%5;
    int waiter_base = W_1 + 200 * waiter_index;

    int back = M[waiter_base+3];  // read the back of the queue
    M[back]= id;  // store customer id in waiter's location
    M[back+1] = count;  // store count in waiter's location
    M[waiter_base+3] = back+2;  // update back of the queue
    M[waiter_base+1]++;  // increase the waiting orders for the waiter

    // printf("Released the mutex lock\n");
    V(mutexid);
        

    vop.sem_num = waiter_index;  // specify which waiter to wake up
    // printf("Customer %d signaling waiter index: %d\n", id, pop.sem_num);
    V(waiterid);   // signal to the waiter to take order

        // place order statement
    print_time();
    printf("Customer %d: Order placed to Waiter %c\n", id, 'U' + (id-1)%5);

    pop.sem_num = id-1;  // specify which customer to wake up
    P(customerid);  // wait for order to come 

    // order is ready statement
    print_time();
    printf("Customer %d: Order is ready\n", id);

    P(mutexid);
    current_time = M[0];
    V(mutexid);

    // eat food, eating takes 30 min time 
    int time_to_sleep = 30 * 100;  // 30 min time
    usleep(time_to_sleep);
    
    P(mutexid);

    if (M[0] < current_time + 30) {
        M[0] = current_time + 30;
    }
    
    M[1]++;   // empty table is increased by 1 

    // eat food statement
    print_time();
    printf("Customer %d finishes eating and leaves\n", id);
    V(mutexid);
}

int main() {

    signal(SIGINT , handler);
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    shmid = shmget(key, 2000 * sizeof(int), 0777 | IPC_CREAT);
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
        if (id == -1) break;
        
        if (arrival_time < last_arrival_time) {
            fprintf(stderr, "Error: Customer %d has an invalid arrival time.\n", id);
            continue;
        }

        // Wait for the time difference between current and last customer
        int delay = (arrival_time - last_arrival_time) * 100;
        if (delay > 0) {
            usleep(delay);
        }
        
        // Update the system time
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

    // Wait for all customer processes to complete
    while (wait(NULL) > 0);

    return 0;
}
