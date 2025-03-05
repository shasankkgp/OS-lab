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
int mutexid,cookid,waiterid,customerid;
struct sembuf pop,vop;

void cmain( int id , int arrival_time , int count ){
    // code for customers 
    if( M[0]> 400 ) return ;
    if( M[1]==0 ) return ;
    while(1){
        P(mutexid);
        M[1]--;
        P(waiterid);   // wait for the waiter
        
    }
}

int main(){
    struct sembuf pop,vop;
    int status;

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    int shmid = shmget(key,2000*sizeof(int),0777|IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    M = (int*)shmat(shmid,(void*)0,0);
    if (M == (void*)-1) {
        perror("shmat failed");
        exit(1);
    }

    mutexid = M[MUTEX];
    cookid = M[MUTEX+1];
    waiterid = M[MUTEX+2];
    customerid = M[MUTEX+3];

    struct customer *customers;

    customers = (struct customer*)malloc(MAX_CUSTOMERS * sizeof(customer));
    if (customers == NULL) {
        perror("malloc failed");
        exit(1);
    }

    FILE *f = fopen("customers.txt","r");
    if (f == NULL) {
        perror("fopen failed");
        exit(1);
    }

    int i = 0;
    while (fscanf(f, "%d %d %d", &customers[i].id, &customers[i].arrival_time, &customers[i].count) == 3) {
        i++;
        if (i >= MAX_CUSTOMERS) {
            break;
        }
    }
    int num_customers = i;

    fclose(f);

    for( int i=0 ; i<num_customers ; i++){
        if(fork()==0){
            cmain(customers[i].id, customers[i].arrival_time, customers[i].count);
            exit(0);
        }else{
            wait(&status);
        }
    }

    free(customers);

    // Detach from shared memory
    if (shmdt(M) == -1) {
        perror("shmdt failed");
        exit(1);
    }

    // Remove shared memory
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failed");
        exit(1);
    }

    // Remove semaphores
    if (semctl(mutexid, 0, IPC_RMID) == -1) {
        perror("semctl (mutexid) failed");
        exit(1);
    }
    if (semctl(cookid, 0, IPC_RMID) == -1) {
        perror("semctl (cookid) failed");
        exit(1);
    }
    if (semctl(waiterid, 0, IPC_RMID) == -1) {
        perror("semctl (waiterid) failed");
        exit(1);
    }
    if (semctl(customerid, 0, IPC_RMID) == -1) {
        perror("semctl (customerid) failed");
        exit(1);
    }

    return 0;
}