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

void print_time() {
    int total_minutes = 11 * 60 + M[0];  // Convert 11:00 AM base to minutes
    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;
    
    // Convert to 12-hour format
    int display_hour = (hours % 12 == 0) ? 12 : hours % 12;
    const char *period = (hours % 24 >= 12) ? "pm" : "am";
    
    printf("[%02d:%02d %s]", display_hour, minutes, period);
    fflush(stdout);
}

// Function to print indentation based on waiter number
void print_indent(int waiter_no) {
    for (int i = 0; i < waiter_no; i++) {
        printf("\t");
        // fflush(stdout);
    }
}

// cookid - 1 semaphore
// waiterid - 5 semaphores
// customerid - 200 semaphores
// mutexid - 1 semaphore

void wmain(int waiter_no) {  // Pass waiter number
    // code for waiters
    int waiter_base = W_1 + 200 * waiter_no;

    // pop.sem_num = 0;
    // P(mutexid);
    // // Set up initial queue pointers if not already done
    // if (M[waiter_base + 2] == 0 && M[waiter_base + 3] == 0) {
    //     // Initialize front and back pointers
    //     M[waiter_base + 2] = waiter_base + 4;  // front
    //     M[waiter_base + 3] = waiter_base + 4;  // back
    // }
    // V(mutexid);

    print_time();
    print_indent(waiter_no);
    printf("Waiter %c is ready\n", 'U' + waiter_no);
    fflush(stdout);

    while (M[0] <= 240 || (M[waiter_base+1]>0) || (M[waiter_base] != -1)) {
        pop.sem_num = waiter_no;  // Specify which semaphore to decrement
        P(waiterid);  // Wait for signal on specific waiter semaphore
        // print_time();
        // printf("Waiter %c woke up\n", 'U' + waiter_no);
        
        pop.sem_num = 0;
        P(mutexid);
        // printf("got the mutex key\n");
        int current_time = M[0];

        if ( M[waiter_base] != -1 ) {  // if cook is the one to wake up 
            int customer_id = M[waiter_base];
            M[waiter_base] = -1;   // clear the flag
            M[waiter_base + 1]--;    // decrease the waiting orders for the waiter
            // print statement
            print_time();
            print_indent(waiter_no);
            printf("Waiter %c: Serving food to Customer %d\n", 'U' + waiter_no, customer_id);
            fflush(stdout);

            vop.sem_num = customer_id-1;   // signal the customer 
            V(customerid);

            // printf("released the mutex lock\n");
            vop.sem_num = 0;
            V(mutexid);

        } else if(M[waiter_base+1]>0){     // if customer is the one to wake up
            // waiter queue starts from W_1 + 200*waiter_no
            // first element is customer id and second element is count
            // third element is front of the queue
            // fourth element is back of the queue
            int front = M[waiter_base+ 2]; 
            int customer_id = M[front];
            int count = M[front + 1];

            M[waiter_base+ 2] = front +2;  // update front of the queue
            
            
            current_time = M[0];

            vop.sem_num = 0;
            V(mutexid);
            // printf("released the mutex lock\n");
            
            usleep(100000); // 100 ms for taking the order

            pop.sem_num = 0;
            P(mutexid);
            if( M[0] < current_time + 1 ){
                M[0] = current_time + 1;   // update the current time
            }

            // print statement
            print_time();
            print_indent(waiter_no);
            printf("Waiter %c: Placing order for Customer %d (count = %d)\n", 'U' + waiter_no, customer_id, count);
            fflush(stdout);

            M[3]++;   // increase in orders pending
            int back = M[C_1 + 1];   // back of cooks queue
            M[back] = waiter_no;  // Store which waiter is serving
            M[back + 1] = customer_id;
            M[back + 2] = count;
            M[C_1 + 1] = back+3;  // update back of cooks queue
            // printf("released the mutrex lock\n");
            vop.sem_num = customer_id - 1;  // signal the customer
            V(customerid); 

            vop.sem_num = 0;
            V(mutexid);

            // printf("released the mutex lock\n");
            // printf("Signaling the cook\n");
            vop.sem_num = 0;
            V(cookid);    // signal the cook
        }else{
            vop.sem_num = 0;
            V(mutexid);
        }
    }
    print_time();
    print_indent(waiter_no);
    printf("Waiter %c leaving (no more customers to serve)\n", 'U' + waiter_no);
    fflush(stdout);
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

    // Fork waiter processes
    for (int i = 0; i < MAX_WAITERS; i++) {
        if (fork() == 0) {
            wmain(i);
            exit(0);
        }
    }
    
    // Wait for all waiter processes to complete
    while (wait(NULL) > 0);
    
    return 0;
}