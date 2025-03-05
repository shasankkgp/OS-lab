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

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

#define MAX_COOKS 2
#define MAX_WAITERS 5
#define MAX_CUSTOMERS 200

#define key 1234

void cmain(){
    // code for cooks 
}

int main(){
    // need to start shared memory of size 2000 ints 
    // 0-99 for storing variables 
    // 100-299 for waiter 1
    // 300-499 for waiter 2
    // 500-699 for waiter 3
    // 700-899 for waiter 4
    // 900-1099 for waiter 5
    // 1100-2000 for cooks

    int shmid = shmget(key,2000*sizeof(int),0777|IPC_CREAT);
    int *shm = (int*)shmat(shmid,(void*)0,0);
    for(int i=0 ; i<2000 ; i++ ){
        shm[i] = 0;
    }
    

    int mutexid,cookid,waiterid,customerid;
    struct sembuf pop,vop;
    int status;

    mutexid = semget(IPC_PRIVATE,1,0777|IPC_CREAT);
    cookid = semget(IPC_PRIVATE,1,0777|IPC_CREAT);
    waiterid = semget(IPC_PRIVATE,MAX_WAITERS,0777|IPC_CREAT);
    customerid = semget(IPC_PRIVATE,MAX_CUSTOMERS,0777|IPC_CREAT); 
    
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    semctl(mutexid,0,SETVAL,1);
    semctl(cookid,0,SETVAL,0);
    for( int i=0 ; i<MAX_WAITERS ; i++ ){
        semctl(waiterid,i,SETVAL,0);
    }
    for( int i=0 ; i<MAX_CUSTOMERS ; i++ ){
        semctl(customerid,i,SETVAL,0);
    }

    for(int i=0 ; i<MAX_COOKS ; i++ ){
        if(fork() == 0){
            cmain();
            exit(0);
        }else{
            wait(&status);
        }
    }
}