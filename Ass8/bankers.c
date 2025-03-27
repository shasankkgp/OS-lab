#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<stdbool.h>

#define MAX_SIZE 100
#define MINUTE_SCALED_TO_USEC 100000

pthread_mutex_t rmtx=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pmtx=PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t BOS,REQB;
pthread_barrier_t ACKB[100];
pthread_cond_t cv[20];
pthread_mutex_t cmtx[20];
int m,n;

int REQ[20];
int MAX[100][20];
int req_type;
int req_th;

int waiting[100];

typedef struct {
    int items[MAX_SIZE];
    int front;
    int rear;
    int size;
} Queue;

// Function to initialize the queue
void initializeQueue(Queue* q){
    q->front = -1;
    q->rear = -1;
    q->size=0;
}

bool isFull(Queue* q) { return (q->size==MAX_SIZE); }

bool isEmpty(Queue* q) { return (q->size==0); }

void enqueue(Queue* q, int value){
    if (isFull(q)) {
        printf("Queue is full\n");
        return;
    }
    if(q->front==-1){
        q->front=0;
    }
    q->rear=(q->rear+1)%MAX_SIZE;
    q->items[q->rear] = value;
    q->size++;
}

int dequeue(Queue* q){
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return -1;
    }
    int value = q->items[q->front];
    if(q->front==q->rear){
        q->front=-1;
        q->rear=-1;   
    }
    else{
        q->front=(q->front+1)%MAX_SIZE;
    }
    q->size--;
    return value;
}

// int peek(Queue* q){
//     if (isEmpty(q)) {
//         printf("Queue is empty\n");
//         return -1; // return some default value or handle
//                    // error differently
//     }
//     return q->items[q->front + 1];
// }

// void printQueue(Queue* q){
//     if (isEmpty(q)) {
//         printf("Queue is empty\n");
//         return;
//     }

//     printf("Current Queue: ");
//     for (int i = q->front + 1; i < q->rear; i++) {
//         printf("%d ", q->items[i]);
//     }
//     printf("\n");
// }

int is_possible(int th,int q_arr[][20],int* AVAILABLE,int ALLOC[][20]){
    int need[n][m];
    for(int i=0;i<n;i++){
        for(int j=0;j<m;j++){
            need[i][j]=MAX[i][j]-ALLOC[i][j];
        }
    }
    for(int i=0;i<m;i++){
        if(q_arr[th][i]>need[th][i]){
            pthread_mutex_lock(&pmtx);
            printf("\t+++ Insufficient resources to grant request of thread %d\n",th);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            return 0;
        }
    }
    for(int i=0;i<m;i++){
        if(q_arr[th][i]>AVAILABLE[i]){
            pthread_mutex_lock(&pmtx);
            printf("\t+++ Insufficient resources to grant request of thread %d\n",th);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            return 0;
        }
    }
#ifdef _DLAVOID
    int finish[n];
    int work[m];
    int temp_avail[m];
    int temp_alloc[n][m];
    for(int i=0;i<m;i++){
        temp_avail[i]=AVAILABLE[i];
    }
    for(int i=0;i<n;i++){
        finish[i]=0;
        for(int j=0;j<m;j++){
            temp_alloc[i][j]=ALLOC[i][j];
        }
    }

    for(int i=0;i<m;i++){
        temp_avail[i]-=q_arr[th][i];
        temp_alloc[th][i]+=q_arr[th][i];
        need[th][i]-=q_arr[th][i];
    }
    for(int i=0;i<m;i++){
        work[i]=temp_avail[i];
    }

    int change=1,key;
    while(change){
        change=0;
        for(int i=0;i<n;i++){
            key=0;
            if(finish[i]==0){
                for(int j=0;j<m;j++){
                    if(need[i][j]>work[j]){
                        key=1;
                        break;
                    }
                }
                if(key==0){
                    finish[i]=1;
                    change=1;
                    for(int j=0;j<m;j++){
                        work[j]+=temp_alloc[i][j];
                    }
                }
            }
        }
    }
    for(int i=0;i<n;i++){
        if(finish[i]==0){
            pthread_mutex_lock(&pmtx);
            printf("\t+++ Unsafe to grant request of thread %d\n",th);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            return 0;
        }
    }
#endif
    return 1;
}

void print_stats(Queue* q,int* alive,int a_cnt,int* avail){
    printf("\t\tWaiting threads:");
    fflush(stdout);
    int len=q->size;
    for(int i=0;i<len;i++){
        int th=dequeue(q);
        printf(" %d",th);
        enqueue(q,th);
    }
    printf("\n");

    printf("%d threads left:",a_cnt);
    for(int i=0;i<n;i++){
        if(alive[i]==1){
            printf(" %d",i);
        }
    }
    printf("\n");

    printf("Available resources:");
    for(int i=0;i<m;i++){
        printf(" %d",avail[i]);
    }
    printf("\n");
    fflush(stdout);
}

void* user_thread(void* arg){
    int* index=(int*)arg;
    int num=*index;
    pthread_mutex_lock(&pmtx);
    printf("\tThread %d born\n",num);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
    char ch;
    int type;
    int sleep_time=0;
    int HELD[m];
    int temp[m];
    int flag=0;
    for(int i=0;i<m;i++){
        MAX[num][i]=0;
        HELD[i]=0;
    }
    char textfile[30];
    sprintf(textfile, "input2/thread%02d.txt",num);

    FILE* fp1=(FILE*)fopen(textfile,"r");
    // if(fp1==NULL){
    //     printf("Error opening file: %s\n",textfile);
    //     return NULL;
    // }
    for(int i=0;i<m;i++){
        fscanf(fp1,"%d",&MAX[num][i]);
    }

    pthread_barrier_wait(&BOS);

    while(1){
        fscanf(fp1,"%d %c",&sleep_time,&ch);
        if(ch=='R'){
            for(int i=0;i<m;i++){
                fscanf(fp1," %d",&temp[i]);
            }
        }
        else if(ch=='Q'){
            for(int i=0;i<m;i++){
                temp[i]=(-1)*HELD[i];
            }
            flag=1;
        }
        type=0;
        for(int i=0;i<m;i++){
            if(temp[i]>0){
                type=1;
                break;
            }
        }
        if(flag==1){
            type=2;
        }
        usleep(sleep_time*MINUTE_SCALED_TO_USEC);
        pthread_mutex_lock(&rmtx);
        req_type=type;
        req_th=num;
        for(int i=0;i<m;i++){
            REQ[i]=temp[i];
        }
        pthread_mutex_lock(&pmtx);
        if(req_type==1){
            printf("\tThread %d sends resource request: type = ADDITIONAL\n",num);
            fflush(stdout);
        }
        else if(req_type==0){
            printf("\tThread %d sends resource request: type = RELEASE\n",num);
            fflush(stdout);
        }
        pthread_mutex_unlock(&pmtx);
        pthread_barrier_wait(&REQB);
        pthread_barrier_wait(&ACKB[num]);
        pthread_mutex_unlock(&rmtx);

        if(type==1){
            pthread_mutex_lock(&cmtx[num]);
            waiting[num]=1;
            pthread_cond_wait(&cv[num],&cmtx[num]);
            waiting[num]=0;
            pthread_mutex_unlock(&cmtx[num]);
            pthread_mutex_lock(&pmtx);
            printf("\tThread %d is granted its last resource request\n",num);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
        }
        else if(type==0){
            pthread_mutex_lock(&pmtx);
            printf("\tThread %d is done with its resource release request\n",num);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
        }
        for(int i=0;i<m;i++){
            HELD[i]+=temp[i];
            temp[i]=0;
        }
        if(flag==1){
            break;
        }
    }
    pthread_mutex_lock(&pmtx);
    printf("\tThread %d going to quit\n",num);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
    pthread_exit(NULL);
}

int main(){
    Queue Q;
    initializeQueue(&Q);
    int alive_cnt;
    int alive[100];

    int AVAILABLE[20];
    int ALLOC[100][20];
    int Q_arr[100][20];
    for(int i=0;i<20;i++){
        AVAILABLE[i]=0;
    }
    for(int i=0;i<100;i++){
        for(int j=0;j<20;j++){
            ALLOC[i][j]=0;
        }
    }
    FILE* fp;
    fp=(FILE*)fopen("input2/system.txt","r");
    if(fp==NULL){
        printf("Failed to read system.txt file\n");
        exit(1);
    }
    fscanf(fp,"%d",&m);
    if(m<5 || m>20){
        printf("Invalid value for m\n");
        fclose(fp);
        exit(1);
    }
    fscanf(fp,"%d",&n);
    if(n<10 || n>100){
        printf("Invalid value for n\n");
        fclose(fp);
        exit(1);
    }
    
    for(int i=0;i<m;i++){
        fscanf(fp,"%d",&AVAILABLE[i]);
    }

    pthread_mutex_init(&rmtx,NULL);
    pthread_mutex_trylock(&rmtx);
    pthread_mutex_unlock(&rmtx); 

    pthread_mutex_init(&pmtx,NULL);
    pthread_mutex_trylock(&pmtx);
    pthread_mutex_unlock(&pmtx);
    
    for(int i=0;i<m;i++){
        pthread_cond_init(&cv[i],NULL);
        pthread_mutex_init(&cmtx[i],NULL);
    }

    if(pthread_barrier_init(&BOS,NULL,n+1)){
        perror("Failed in BOS barrier initialization");
        exit(1);
    }

    if(pthread_barrier_init(&REQB,NULL,2)){
        perror("Failed in REQB barrier initialization");
        exit(1);
    }

    for(int i=0;i<n;i++){
        if(pthread_barrier_init(&ACKB[i],NULL,2)){
            perror("Failed in ACKB barrier initialization");
            exit(1);
        }
    }

    for(int i=0;i<n;i++){
        waiting[i]=0;
    }

    for(int i=0;i<n;i++){
        alive[i]=1;
    }
    alive_cnt=n;

    for(int i=0;i<m;i++){
        REQ[i]=0;
    }

    pthread_t user_th[n];
    int user_num[n];
    for(int i=0;i<n;i++){
        user_num[i]=i;
    }
    for(int i=0;i<n;i++){
        pthread_create(&user_th[i],NULL,&user_thread,(void*)(user_num+i));
    }

    pthread_barrier_wait(&BOS);

    while(1){
        pthread_barrier_wait(&REQB);
        if(req_type==1){
            for(int i=0;i<m;i++){
                if(REQ[i]<0){
                    ALLOC[req_th][i]+=REQ[i];
                    AVAILABLE[i]-=REQ[i];
                    REQ[i]=0;
                }
                Q_arr[req_th][i]=REQ[i];
                REQ[i]=0;
            }
            enqueue(&Q,req_th);
            pthread_mutex_lock(&pmtx);
            printf("Master thread stores resource request of thread %d\n",req_th);
            printf("\t\tWaiting threads:");
            int len=Q.size;
            for(int i=0;i<len;i++){
                int th=dequeue(&Q);
                printf(" %d",th);
                enqueue(&Q,th);
            }
            printf("\n");
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
        }
        else{
            for(int i=0;i<m;i++){
                ALLOC[req_th][i]+=REQ[i];
                AVAILABLE[i]-=REQ[i];
                REQ[i]=0;
            }
        }
        if(req_type==2){
            alive[req_th]=0;
            alive_cnt--;
            pthread_mutex_lock(&pmtx);
            printf("Master thread releases resources of thread %d\n",req_th);
            fflush(stdout);
            print_stats(&Q,alive,alive_cnt,AVAILABLE);
            pthread_mutex_unlock(&pmtx);
        }
        pthread_barrier_wait(&ACKB[req_th]);
        if(alive_cnt==0){
            break;
        }
        pthread_mutex_lock(&pmtx);
        printf("Master thread tries to grant pending requests\n");
        fflush(stdout);
        pthread_mutex_unlock(&pmtx);
        int q_size=Q.size;
        for(int i=0;i<q_size;i++){
            int th=dequeue(&Q);
            if(is_possible(th,Q_arr,AVAILABLE,ALLOC)){
                for(int i=0;i<m;i++){
                    AVAILABLE[i]-=Q_arr[th][i];
                    ALLOC[th][i]+=Q_arr[th][i];
                    Q_arr[th][i]=0;
                }
                pthread_mutex_lock(&pmtx);
                printf("Master thread grants resource request for thread %d\n",th);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
                while(waiting[th]==0)
                {
                }
                pthread_mutex_lock(&cmtx[th]);
                pthread_cond_signal(&cv[th]);
                pthread_mutex_unlock(&cmtx[th]);
            }
            else{
                enqueue(&Q,th);
            }
        }
        pthread_mutex_lock(&pmtx);
        printf("\t\tWaiting threads:");
        int len=Q.size;
        for(int i=0;i<len;i++){
            int th=dequeue(&Q);
            printf(" %d",th);
            enqueue(&Q,th);
        }
        printf("\n");
        fflush(stdout);
        pthread_mutex_unlock(&pmtx);
    }

    for(int i=0;i<n;i++){
        pthread_join(user_th[i],NULL);
    }
    pthread_mutex_destroy(&rmtx);
    pthread_mutex_destroy(&pmtx);
    for(int i=0;i<n;i++){
        pthread_cond_destroy(&cv[i]);
        pthread_mutex_destroy(&cmtx[i]);
    }
    pthread_barrier_destroy(&BOS);
    pthread_barrier_destroy(&REQB);
    for(int i=0;i<n;i++){
        pthread_barrier_destroy(&ACKB[i]);
    }
    return 0;
}