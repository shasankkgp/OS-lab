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

#define P(s) semop(s, &pop, 1)  // wait 
#define V(s) semop(s, &vop, 1)   // signal

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



int *M;
int mutexid, cookid, waiterid, customerid;
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

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void cmain(int cook_no) {
    char cook_name = 'C' + cook_no;

    // code for cooks 
    print_time();
    if (cook_no == 0) {
        printf(" Cook %c is ready\n", cook_name);
    } else {
        printf(" \tCook %c is ready\n", cook_name);
    }

    while (M[0] <= 240 || M[2]>0) {   // repeat part done
        P(cookid);     // wait untill woken up , which waiter have woke me up ? 
        // print_time();
        // printf("Cook %c: Woke up\n",'C' + cook_no);
        P(mutexid);
        // printf("Got the mutex key\n");

        if (M[2]<=0) {
            V(mutexid);
            continue;
        }

        int current_time = M[0];

        int front = M[C_1];    // read cooking request 
        int waiter_no = M[front];
        int customer_id = M[front + 1];
        int count = M[front + 2];

        // if (customer_id <= 0 || count <= 0) {
        //     // Invalid order, skip it
        //     V(mutexid);
        //     continue;
        // }

        M[C_1] = front+3;   // update front of the queue
        M[2]--;   // decrease the orders pending
        
        

        int cook_time = count * 5;   // preparing food for each time

        // print statement
        print_time();
        if (cook_no == 0) {
            printf(" Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n", 
                   cook_name, 'U' + waiter_no, customer_id, count);
        } else {
            printf(" \tCook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n", 
                   cook_name, 'U' + waiter_no, customer_id, count);
        }
        
        V(mutexid);

        usleep(cook_time*100000);

        P(mutexid);
        // printf("Got the mutex key\n");

        if (M[0] < current_time + cook_time) {
            M[0] = current_time + cook_time;
        }

        //print statement
        print_time();
        if (cook_no == 0) {
            printf(" Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n", 
                   cook_name, 'U' + waiter_no, customer_id, count);
        } else {
            printf(" \tCook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n", 
                   cook_name, 'U' + waiter_no, customer_id, count);
        }

        M[W_1+200*waiter_no] = customer_id;   // customer id is stored in the waiter's location 

        

        vop.sem_num = waiter_no;    // signal the exact waiter
        // printf("Signaling waiter %c\n", 'U' + waiter_no);
        V(waiterid);        // signaling the waiter

        V(mutexid);
        
    }   

    print_time();
    if (cook_no == 0) {
        printf(" Cook %c: Leaving\n", cook_name);
    } else {
        printf(" \tCook %c: Leaving\n", cook_name);
    }

    if (cook_no == MAX_COOKS - 1) {
        // printf("All cooks are leaving\n");
        for (int i = 0; i < MAX_WAITERS; i++) {
            vop.sem_num = i;   
            V(waiterid);    // signal all the waiters
        }
    }
}

int main() {
    // need to start shared memory of size 2000 ints 
    // 0-99 for storing variables 
    // 100-299 for waiter 1
    // 300-499 for waiter 2
    // 500-699 for waiter 3
    // 700-899 for waiter 4
    // 900-1099 for waiter 5
    // 1100-2000 for cooks
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;
    
    // Create shared memory segment
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
    
    // Initialize shared memory
    // Enter all these variables in first 100 locations of shared memory
    // M[0] = time, M[1] = number of empty tables, M[2] = number of orders pending, M[3]-next wieter id,M[4]-M[12] = table status
    M[0] = 0;           // Current time
    M[1] = 10;          // Empty tables
    M[2] = 0;           // Orders pending for cooks
    M[3] = 0;           // Next weiter id
    
    // Initialize cook queue
    M[C_1] = C_1 + 2;   // Front pointer
    M[C_1 + 1] = C_1 + 2; // Back pointer
    
    // Initialize waiter memory areas
    for (int i = 0; i < MAX_WAITERS; i++) {
        int base = W_1 + 200 * i;
        M[base] = -1;           // No food ready flag
        M[base + 1] = 0;        // No waiting orders
        M[base + 2] = base + 4; // Front pointer
        M[base + 3] = base + 4; // Back pointer
    }
    
    // Create semaphores
    mutexid = semget(key + 1, 1, 0777 | IPC_CREAT);
    cookid = semget(key + 2, 1, 0777 | IPC_CREAT);
    waiterid = semget(key + 3, MAX_WAITERS, 0777 | IPC_CREAT);
    customerid = semget(key + 4, MAX_CUSTOMERS, 0777 | IPC_CREAT);
    
    // Initialize semaphores
    semctl(mutexid, 0, SETVAL, 1);
    semctl(cookid, 0, SETVAL, 0);
    for (int i = 0; i < MAX_WAITERS; i++) {
        semctl(waiterid, i, SETVAL, 0);
    }
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        semctl(customerid, i, SETVAL, 0);
    }
    
    // Store semaphore IDs in shared memory for other processes
    M[MUTEX] = mutexid;
    M[MUTEX + 1] = cookid;
    M[MUTEX + 2] = waiterid;
    M[MUTEX + 3] = customerid;
    
    // Fork cook processes
    for (int i = 0; i < MAX_COOKS; i++) {
        if (fork() == 0) {
            cmain(i);
            exit(0);
        }
    }
    
    // Wait for all cook processes to complete
    while (wait(NULL) > 0);
    
    // Do not clean up IPC resources here - leave it to customer.c
    return 0;
}