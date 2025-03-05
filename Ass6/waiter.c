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

int *M;
int mutexid,cookid,waiterid,customerid;
struct sembuf pop,vop;

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

void wmain(){
    // code for waiters 
    while(M[0]<400){
        P(waiterid);
        P(mutexid);
        if( M[W_1]!= -1 ){
            // cook wakes up the waiter
            int customer_id = M[W_1];
            M[W_1]=-1;
            vop.sem_num=customer_id;
            V(customerid);
            V(mutexid);
        }else{
            // customer wakes up the waiter 
            int front = M[W_1+2];
            int customer_id = M[front];
            int count = M[front+1];
            M[W_1+2] = (front+2)%2000;
            usleep(100); // 100 ms for taking the order
            int back = M[C_1+1];
            M[back] = 1;     // waiter number, should have to change after 
            M[back+1] = customer_id;
            M[back+2] = count;
            M[C_1+1] = (back+3)%2000;
            V(mutexid);
            V(cookid);    // signals to the cooks 
        }
    }
}

int main(){
    
    int status;

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;    
    vop.sem_op = 1;

    int shmid = shmget(key,2000*sizeof(int),0777|IPC_CREAT);
    M = (int*)shmat(shmid,(void*)0,0);


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