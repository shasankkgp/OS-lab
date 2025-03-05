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

#define key 1234;

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

void wmain(){
    // code for waiters 
}

int main(){
    struct sembuf pop,vop;
    int status;

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    int shmid = shmget(key,2000*sizeof(int),0777|IPC_CREAT);
    int *M = (int*)shmat(shmid,(void*)0,0);

    int mutexid,cookid,waiterid,customerid;

    mutexid = M[MUTEX];
    cookid = M[MUTEX+1];
    waiterid = M[MUTEX+2];
    customerid = M[MUTEX+3];

    

    for(int i=0 ; i<MAX_WAITERS ; i++){
        if(fork()==0){
            wmain();
            exit(0);
        }else{
            wait(&status);
        }
    }
}