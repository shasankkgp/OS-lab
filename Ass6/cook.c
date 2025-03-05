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
int mutexid,cookid,waiterid,customerid;
struct sembuf pop,vop;

void cmain(){
    // code for cooks 
    while( M[0]< 400 ){
        P(cookid);
        int front = M[C_1+2];
        int waiter_no = M[front];
        int customer_id = M[front+1];
        int count = M[front+2];
        // print statement
        P(mutexid);
        M[2]--;
        M[C_1] = (M[C_1]+3)%2000;
        V(mutexid);
        // sleep for 5*count seconds
        int time_to_sleep = count*5*100;
        usleep(time_to_sleep);
        vop.sem_num = waiter_no;
        M[C_1] = customer_id;   // have to change this 
        V(waiterid);
    }   
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
    M = (int*)shmat(shmid,(void*)0,0);
    for(int i=0 ; i<2000 ; i++ ){
        M[i] = -1;
    }
    
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
    // Enter all these variables in first 100 locations of shared memory
    // M[0] = time , M[1] = number of empty tables , M[2] = number of orders pending , M[3]-M[12] = table status
    M[0] = 0;
    M[1] = 10;
    M[2] = 0;
    M[3] = 0;
    M[MUTEX] = mutexid;
    M[MUTEX+1] = cookid;
    M[MUTEX+2] = waiterid;
    M[MUTEX+3] = customerid;

    for( int i=0 ; i<MAX_COOKS ; i++ ){
        if(fork() == 0){
            cmain();
            exit(0);
        }else{
            wait(&status);
        }
    }
}