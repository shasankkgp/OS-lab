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

int *M;
int mutexid, cookid, waiterid, customerid;
struct sembuf pop, vop;

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

void print_time(){
    printf("[%02d:%02d am]",11+M[0]/60,M[0]%60);
}

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void wmain(int waiter_no) {  // Pass waiter number
    while (M[0] < 240) {
        pop.sem_num = waiter_no;  // Specify which semaphore to decrement
        P(waiterid);  // Wait for signal on specific waiter semaphore
        
        P(mutexid);
        int current_time = M[0];
        if (M[W_1+200*waiter_no] != -1) {  // if cook is the one to wake up 
            int customer_id = M[W_1+200*waiter_no];
            M[W_1+200*waiter_no] = -1;
            // print statement
            print_time();
            printf("Waiter %c: Serrving food to Customer %d\n", 'U' + waiter_no, customer_id);

            vop.sem_num = customer_id;   // signal the customer 
            V(customerid);
            V(mutexid);
        } else {     // if customer is the one to wake up
            int front = M[W_1+200*waiter_no+ 2]; 
            int customer_id = M[front];
            int count = M[front + 1];
            M[W_1+200*waiter_no + 2] = (front + 2) % 2000;
            M[W_1+200*waiter_no + 1]--;
            // print statement
            print_time();
            printf("Waiter %c: Placing order for Customer %d (count = %d)\n", 'U' + waiter_no, customer_id, count);

            usleep(100); // 100 ms for taking the order
            M[0] = current_time + 1;  // update the current time
            M[3]++;   // increase in orders pending
            int back = M[C_1 + 1];   // back of cooks queue
            M[back] = waiter_no;  // Store which waiter is serving
            M[back + 1] = customer_id;
            M[back + 2] = count;
            M[C_1 + 1] = (back + 3) % 2000;  // update back of cooks queue
            V(cookid);    // signal the cook
            V(mutexid);
        }
    }
    print_time();
    printf("Waiter %c leaving (no more customers to serve)\n", 'U' + waiter_no);
}

int main() {
    
    // int status;

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    int shmid = shmget(key, 2000 * sizeof(int), 0777 | IPC_CREAT);
    M = (int*)shmat(shmid, (void*)0, 0);

    mutexid = M[MUTEX];
    cookid = M[MUTEX + 1];
    waiterid = M[MUTEX + 2];
    customerid = M[MUTEX + 3];

    for (int i = 0; i < MAX_WAITERS; i++) {
        if (fork() == 0) {
            wmain(i);  // Pass unique waiter_no (0 to 4)
            exit(0);
        }
    }
    
}