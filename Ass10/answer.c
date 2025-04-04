#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<time.h>

#define MAX_n 500
#define MIN_n 50
#define MIN_m 10
#define MAX_m 100
#define PGS_PER_PROC 2048
#define INT_PER_PAGE 1024
#define USER_FRAMES 12288
#define ESS_PGS 10
#define NFFMIN 1000

typedef struct node{
    int frame_num;
    int last_owner;
    int last_page;
}FFLIST_node;

int n,m;
int type=-1;
uint16_t page_tables[MAX_n][PGS_PER_PROC];
uint16_t page_history[MAX_n][PGS_PER_PROC];
int proc_active[MAX_n];
int n_searches_done[MAX_n];
int search_indices[MAX_n][MAX_m];
int proc_A_size[MAX_n];

int n_access[MAX_n];
int n_faults[MAX_n];
int n_swaps[MAX_n];
int attempt_counts[MAX_n][4];

FFLIST_node *FFLIST;
int NFF;

typedef struct{
    int* data;
    int front;
    int rear;
    int capacity;
    int size;
}Queue;

Queue create_queue(int capacity){
    Queue q;
    q.data=(int*)malloc(capacity*sizeof(int));
    if(!q.data){
        fprintf(stderr,"Memory allocation failed for queue\n");
        exit(1);
    }
    q.front=-1;
    q.rear=-1;
    q.capacity=capacity;
    q.size=0;
    return q;
}

int is_empty(Queue* q){
    return q->size==0;
}

int is_full(Queue* q){
    return q->size==q->capacity;
}

void enqueue(Queue* q,int p){
    if(is_full(q)){
        fprintf(stderr,"Queue full,enqueue failed\n");
        return;
    }
    q->rear=(q->rear+1)%q->capacity;
    q->data[q->rear]=p;
    if(q->front==-1){
        q->front=q->rear;
    }
    q->size++;
    return;
}

int dequeue(Queue* q){
    if(is_empty(q)){
        fprintf(stderr,"Queue empty,dequeue failed\n");
        return -1;
    }
    int p=q->data[q->front];
    q->front=(q->front+1)%q->capacity;
    q->size--;
    if(q->size==0){
        q->front=-1;
        q->rear=-1;
    }
    return p;
}

void add_to_FFLIST_back(int frame_num,int last_owner,int last_page){
    if(NFF>=USER_FRAMES){
        fprintf(stderr,"FFLIST full,cannot add more frames\n");
        return;
    }
    FFLIST[NFF].frame_num=frame_num;
    FFLIST[NFF].last_owner=last_owner;
    FFLIST[NFF].last_page=last_page;
    NFF++;
    return;
}

void add_to_FFLIST_front(int frame_num,int last_owner,int last_page){
    if(NFF>=USER_FRAMES){
        fprintf(stderr,"FFLIST full,cannot add more frames\n");
        return;
    }
    for(int i=NFF;i>0;i--){
        FFLIST[i]=FFLIST[i-1];
    }
    FFLIST[0].frame_num=frame_num;
    FFLIST[0].last_owner=last_owner;
    FFLIST[0].last_page=last_page;
    NFF++;
    return;
}

void remove_from_FFLIST(int index){
    if(index<0 || index>=NFF){
        fprintf(stderr,"Invalid FFLIST index\n");
        return;
    }
    for(int i=index;i<NFF-1;i++){
        FFLIST[i]=FFLIST[i+1];
    }
    NFF--;
    return;
}

void load_ess_pages(int p_idx){
    for(int j=0;j<ESS_PGS;j++){
        page_history[p_idx][j]=(uint16_t)0xFFFF;
        int f_num=FFLIST[0].frame_num;
        page_tables[p_idx][j]=(uint16_t)(f_num|(1<<15));
        remove_from_FFLIST(0);
    }
    return;
}

int find_frame_for_page_fault(int p_idx,int pg_num){
    // Attempt 1: Check if the page was previously owned by this process
    for(int i=NFF-1;i>=0;i--){
        if(FFLIST[i].last_owner==p_idx && FFLIST[i].last_page==pg_num){
            int frame_num=FFLIST[i].frame_num;
            remove_from_FFLIST(i);
            attempt_counts[p_idx][0]++;
            type=1;
            #ifdef VERBOSE
            printf("        Attempt 1: Page found in free frame %d\n",frame_num);
            #endif
            return frame_num;
        }
    }
    
    // Attempt 2: Find a frame with no owner
    for(int i=NFF-1;i>=0;i--){
        if(FFLIST[i].last_owner==-1){
            int frame_num=FFLIST[i].frame_num;
            remove_from_FFLIST(i);
            attempt_counts[p_idx][1]++;
            type=2;
            #ifdef VERBOSE
            printf("        Attempt 2: Free frame %d owned by no process found\n",frame_num);
            #endif
            return frame_num;
        }
    }
    
    // Attempt 3: Find a frame previously owned by this process
    for(int i=NFF-1;i>=0;i--){
        if(FFLIST[i].last_owner==p_idx){
            int frame_num=FFLIST[i].frame_num;
            type=3;
            #ifdef VERBOSE
            printf("        Attempt 3: Own page %d found in free frame %d\n",FFLIST[i].last_page,frame_num);
            #endif
            remove_from_FFLIST(i);
            attempt_counts[p_idx][2]++;
            return frame_num;
        }
    }
    
    // Attempt 4: Pick a random frame from FFLIST
    int random_index=rand()%NFF;
    int frame_num=FFLIST[random_index].frame_num;
    type=4;
    #ifdef VERBOSE
    printf("        Attempt 4: Free frame %d owned by Process %d chosen\n",frame_num,FFLIST[random_index].last_owner);
    #endif
    remove_from_FFLIST(random_index);
    attempt_counts[p_idx][3]++;
    return frame_num;
}

int find_victim_page(int p_idx){
    uint16_t min_history=0xFFFF;
    int victim_page=-1;
    for(int i=ESS_PGS;i<PGS_PER_PROC;i++){
        if(!(page_tables[p_idx][i] & (1<<15))){
            continue;
        }
        if(page_history[p_idx][i]<min_history){
            min_history=page_history[p_idx][i];
            victim_page=i;
        }
    }
    return victim_page;
}

void return_frames(int p_idx){
    for(int i=0;i<PGS_PER_PROC;i++){
        if(page_tables[p_idx][i] & (1<<15)){
            int frame_num=page_tables[p_idx][i] & ~((1<<15) | (1<<14));
            add_to_FFLIST_back(frame_num,-1,-1);
            page_tables[p_idx][i]=(uint16_t)0;
        }
    }
    return;
}

void update_history(int p_idx){
    for(int i=0;i<PGS_PER_PROC;i++){
        if(page_tables[p_idx][i] & (1<<15)){
            page_history[p_idx][i]=(page_history[p_idx][i] >> 1) | ((page_tables[p_idx][i] & (1<<14)) ? 0x8000:0);
            page_tables[p_idx][i]&=(~(1<<14));
        }
    }
    return;
}

void binary_search(int p_idx){
    int L=0;
    int R=proc_A_size[p_idx] - 1;
    int num=search_indices[p_idx][n_searches_done[p_idx]];
    #ifdef VERBOSE
    printf("+++ Process %d: Search %d\n",p_idx,n_searches_done[p_idx]+1);
    #endif
    
    while(L<R){
        int M=(L+R)/2;
        int pg_num=ESS_PGS+(M/INT_PER_PAGE);
        n_access[p_idx]++;
        if(page_tables[p_idx][pg_num] & (1<<15)){//valid page
            page_tables[p_idx][pg_num]|=(1<<14);
        }
        else{//page fault
            n_faults[p_idx]++;
            #ifdef VERBOSE
            printf("    Fault on Page %4d: ",pg_num);
            #endif
            if(NFF>NFFMIN){
                int frame_num=FFLIST[0].frame_num;
                remove_from_FFLIST(0);
                page_tables[p_idx][pg_num]=(uint16_t)(frame_num | (1<<15) | (1<<14));
                page_history[p_idx][pg_num]=(uint16_t)0xFFFF;
                #ifdef VERBOSE
                printf("Free frame %d found\n",frame_num);
                #endif
            }
            else if(NFF==NFFMIN){
                n_swaps[p_idx]++;
                int victim_page=find_victim_page(p_idx);
                int old_frame=page_tables[p_idx][victim_page] & ~((1<<15) | (1<<14));
                #ifdef VERBOSE
                printf("To replace Page %d at Frame %d [history = %d]\n",victim_page,old_frame,page_history[p_idx][victim_page]);
                #endif
                page_tables[p_idx][victim_page]=(uint16_t)0;
                int new_frame=find_frame_for_page_fault(p_idx,pg_num);
                page_tables[p_idx][pg_num]=(uint16_t)(new_frame | (1<<15) | (1<<14));
                page_history[p_idx][pg_num]=(uint16_t)0xFFFF;

                if(type==2){
                    add_to_FFLIST_front(old_frame,p_idx,victim_page);
                }
                else{
                    add_to_FFLIST_back(old_frame,p_idx,victim_page);
                }
                type=-1;
            }
        }
        if(num<=M){
            R=M;
        }
        else{
            L=M+1;
        }
    }
    update_history(p_idx);
    n_searches_done[p_idx]++;
    return;
}

int main(){
    srand(time(NULL));
    
    FILE* fp=(FILE*)fopen("search.txt","r");
    if(fp==NULL){
        perror("Error opening input file");
        exit(1);
    }
    fscanf(fp,"%d %d",&n,&m);
    // printf("n=%d ; m=%d\n",n,m);
    
    if(n>MAX_n || n<MIN_n){
        printf("Invalid value forn: %d<=n<=%d\n",MIN_n,MAX_n);
        fclose(fp);
        exit(1);
    }
    if(m>MAX_m || m<MIN_m){
        printf("Invalid value form: %d<=m<=%d\n",MIN_m,MAX_m);
        fclose(fp);
        exit(1);
    }
    
    for(int i=0;i<n;i++){
        proc_active[i]=1;
        fscanf(fp,"%d",&proc_A_size[i]);
        // printf("A_size=%d; ",proc_A_size[i]);
        for(int j=0;j<m;j++){
            fscanf(fp," %d",&search_indices[i][j]);
            // printf("%d ",search_indices[i][j]);
        }
        // printf("\n");
    }
    fclose(fp);

    for(int i=0;i<MAX_n;i++){
        for(int j=0;j<PGS_PER_PROC;j++){
            page_tables[i][j]=(uint16_t)0;
            page_history[i][j]=(uint16_t)0;
        }
        proc_active[i]=0;
        n_searches_done[i]=0;
        n_access[i]=0;
        n_faults[i]=0;
        n_swaps[i]=0;
        for(int j=0;j<4;j++){
            attempt_counts[i][j]=0;
        }
    }
    
    Queue RQ=create_queue(MAX_n);
    FFLIST=(FFLIST_node*)malloc(USER_FRAMES*sizeof(FFLIST_node));
    if(!FFLIST){
        fprintf(stderr,"Memory allocation failed forFFLIST\n");
        exit(1);
    }
    
    NFF=0;
    for(int i=0;i<USER_FRAMES;i++){
        add_to_FFLIST_back(i,-1,-1);
    }
    
    for(int i=0;i<n;i++){
        load_ess_pages(i);
        enqueue(&RQ,i);
    }
    
    while(!is_empty(&RQ)){
        int p_idx=dequeue(&RQ);
        binary_search(p_idx);
        if(n_searches_done[p_idx]>=m){
            proc_active[p_idx]=0;
            return_frames(p_idx);
        }
        else{
            enqueue(&RQ,p_idx);
        }
    }
    
    int tot_access=0;
    int tot_faults=0;
    int tot_swaps=0;
    int tot_attempts[4]={0,0,0,0};
    printf("+++ Page access summary\n");
    printf("    PID     Accesses        Faults         Replacements                        Attempts\n");   
    
    for(int i=0;i<n;i++){
        printf("    %-9d ",i);
        printf("%-9d ",n_access[i]);
        printf("%-5d (%4.2f%%)",n_faults[i],(n_access[i]>0 ? (n_faults[i]*100.0/n_access[i]):0.0));
        printf("    %-5d (%4.2f%%) ",n_swaps[i],(n_access[i]>0 ? (n_swaps[i]*100.0/n_access[i]):0.0));
        printf("%6d + %3d + %3d + %3d  ",attempt_counts[i][0],attempt_counts[i][1],attempt_counts[i][2],attempt_counts[i][3]);
        printf("(%3.2f%% + ",(n_swaps[i]>0 ? (attempt_counts[i][0]*100.0/n_swaps[i]):0.0));
        printf("%3.2f%% + ",(n_swaps[i]>0 ? (attempt_counts[i][1]*100.0/n_swaps[i]):0.0));
        printf("%4.2f%% + ",(n_swaps[i]>0 ? (attempt_counts[i][2]*100.0/n_swaps[i]):0.0));
        printf("%3.2f%%)\n",(n_swaps[i]>0 ? (attempt_counts[i][3]*100.0/n_swaps[i]):0.0));
        
        tot_access+=n_access[i];
        tot_faults+=n_faults[i];
        tot_swaps+=n_swaps[i];
        for(int j=0;j<4;j++){
            tot_attempts[j]+=attempt_counts[i][j];
        }
    }

    printf("\n    Total     ");
    printf("%-9d ",tot_access);
    printf("%-6d(%4.2f%%)",tot_faults,(tot_access>0 ? (tot_faults*100.0/tot_access):0.0));
    printf("    %-6d(%4.2f%%) ",tot_swaps,(tot_access>0 ? (tot_swaps*100.0/tot_access):0.0));
    printf("%6d + %4d + %5d + %d  ",tot_attempts[0],tot_attempts[1],tot_attempts[2],tot_attempts[3]);
    printf("(%3.2f%% + ",(tot_swaps>0 ? (tot_attempts[0]*100.0/tot_swaps):0.0));
    printf("%3.2f%% + ",(tot_swaps>0 ? (tot_attempts[1]*100.0/tot_swaps):0.0));
    printf("%4.2f%% + ",(tot_swaps>0 ? (tot_attempts[2]*100.0/tot_swaps):0.0));
    printf("%3.2f%%)\n",(tot_swaps>0 ? (tot_attempts[3]*100.0/tot_swaps):0.0));
    
    free(FFLIST);
    free(RQ.data);
    return 0;
}