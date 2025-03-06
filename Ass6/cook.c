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

void print_time(){
    printf("[%02d:%02d am]",11+M[0]/60,M[0]%60);
}

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void cmain( int cook_no) {
    // code for cooks 
    print_time();
    printf("Cook %c is ready\n",'C' + cook_no);

    while (M[0] < 240) {   // repeat part done
        P(cookid);     // wait untill woken up , which waiter have woke me up ? 
        P(mutexid);

        int front = M[C_1];    // read cooking request 
        int waiter_no = M[front];
        int customer_id = M[front + 1];
        int count = M[front + 2];

        // print statement
        print_time();
        printf("Cook %c: Prepating order (Waiter %c, Customer &=%d, Count %d)\n", 'C' + cook_no, 'U' + waiter_no, customer_id, count);
       
        int current_time = M[0];
        M[2]--;    // orders pending is reduced by 1
        M[C_1] = (M[C_1] + 3) % 2000;   // front is updated
        V(mutexid);

        // sleep for 5*count seconds
        int time_to_sleep = count * 5 * 100;     // preparing food for each time 
        usleep(time_to_sleep);
        P(mutexid);
        //print statement
        print_time();
        printf("Cook %c: Prepared order (Waiter %c,Customer %d, Count %d)\n", 'C' + cook_no, 'U' + waiter_no, customer_id, count);

        M[0] = current_time + count * 5;   // update the current time
        vop.sem_num = waiter_no;    // signal the exact waiter
        M[W_1+200*waiter_no] = customer_id;   // customer id is stored in the waiter's location 
        V(waiterid);        // signaling the waiter
        V(mutexid);
    }   

    print_time();
    printf("Cook %c: leaving\n",'C' + cook_no);
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

    int shmid = shmget(key, 2000 * sizeof(int), 0777 | IPC_CREAT);
    M = (int*)shmat(shmid, (void*)0, 0);
    for (int i = 0; i < 2000; i++) {
        M[i] = -1;
    }
    
    // int status;

    key_t sem_key = ftok("cook.c", 'S'); // Generate unique key
    mutexid = semget(sem_key, 1, 0666 | IPC_CREAT);
    cookid = semget(sem_key + 1, 1, 0666 | IPC_CREAT);
    waiterid = semget(sem_key + 2, MAX_WAITERS, 0666 | IPC_CREAT);
    customerid = semget(sem_key + 3, MAX_CUSTOMERS, 0666 | IPC_CREAT);

    if (mutexid == -1 || cookid == -1 || waiterid == -1 || customerid == -1) {
        perror("semget failed");
        exit(1);
    }

    
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    semctl(mutexid, 0, SETVAL, 1);
    semctl(cookid, 0, SETVAL, 0);
    for (int i = 0; i < MAX_WAITERS; i++) {
        semctl(waiterid, i, SETVAL, 0);
    }
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        semctl(customerid, i, SETVAL, 0);
    }
    // Enter all these variables in first 100 locations of shared memory
    // M[0] = time, M[1] = number of empty tables, M[2] = number of orders pending, M[3]-M[12] = table status
    M[0] = 0;
    M[1] = 10;
    M[2] = 0;
    M[3] = 0;
    M[MUTEX] = mutexid;
    M[MUTEX + 1] = cookid;
    M[MUTEX + 2] = waiterid;
    M[MUTEX + 3] = customerid;

    for (int i = 0; i < MAX_COOKS; i++) {
        if (fork() == 0) {
            cmain(i);
            exit(0);
        } else {
            // wait(&status);
        }
    }
}