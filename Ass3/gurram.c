#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 1000
#define MAX 1e9
#define MIN 1e-9
const char* filename = "proc.txt";
char* outfile = "output1.txt";
int n;
int curr_time = 0;
int* wait_time;
FILE* out;

int roundoff(double d){
    int num = d;
    if(d-num-0.5 > MIN){
        return num+1;
    }
    return num;
}
typedef struct process{
    int id;
    int arrival_time;
    int event_time;
    int index;
    int ind;
    int size;
    int arr[SIZE];
    int done[SIZE];
    int completed;
}proc;

proc* arr;



int getStatus(int ind){
    int index = arr[ind].index;
    if(arr[ind].completed){
        return 5;   //Process completed
    }
    else if(arr[ind].event_time == arr[ind].arrival_time){
        return 1;   //first arrival
    }
    
    else if(arr[ind].index%2==0){
        if(arr[ind].done[index]!=0){
            return 3;   //Preempted process
        }
        else{
            return 2;   //Completion of I/O(Rearrival)
        }
    }
    return 4;   //I/O burst
}



typedef struct linked_list{
    int index;
    struct linked_list* next;
}ll;

typedef struct Q{
    ll* front;
    ll* back;
}Queue;

typedef Queue* queue;

queue init(){
    queue q = (queue)malloc(sizeof(Queue));
    q->front = NULL;
    q->back = NULL;
    return q;
}

queue enqueue(queue q,int ind){
    ll* node = (ll *)malloc(sizeof(ll));
    node->index = ind;
    node->next = NULL;
    if(q->front==NULL){
        q->front = q->back = node;
    }
    else{
        q->back->next = node;
        q->back = node;
    }
    return q;
}

queue deque(queue q){
    if(q->front==NULL){
        printf("Underflow!\n");
        exit(2);
    }
    ll* dummy = q->front;
    q->front = q->front->next;
    free(dummy);
    return q;
}

int front(queue q){
    if(q->front==NULL){
        printf("Underflow!\n");
        exit(3);
    }
    return q->front->index;
}

int empty(queue q){
    return (q->front == NULL);
}

// Declare a heap structure
struct Heap {
    int* arr;
    int size;
    int capacity;
};
 
// define the struct Heap name
typedef struct Heap heap;

void print_status(int ind){
    int tat = arr[ind].event_time - arr[ind].arrival_time;

    int wt = 0,tbt = 0;
    for(int i=0;i<arr[ind].size;i++){
        tbt += arr[ind].arr[i];
    }

    wt = tat - tbt;
    int percent = roundoff(100.0*(tat)/(tbt));
    fprintf(out,"%-10d: Process%7d exits. Turnaround time = %4d (%d%%), Wait time = %d\n",arr[ind].event_time,ind+1,tat,percent,wt);
    wait_time[ind] = wt;
}


int compare(proc p1,proc p2){
    if(p1.event_time != p2.event_time){
        return p1.event_time < p2.event_time; 
    }
    int io1 = p1.index%2,io2 = p2.index%2;
    if(io1 != io2){
        if(io1 == 0){
            return 1;
        }
        else{
            return 0;
        }
    }
    return p1.id < p2.id;
}


 
// forward declarations
heap* createHeap(int capacity, proc* processes);
void insertHelper(heap* h, int index);
void heapify(heap* h, int index);
int extractMin(heap* h);
void insert(heap* h, int data);
int Empty(heap* h);


int Empty(heap* h){
    return (h->size==0);
}
// Define a createHeap function
heap* createHeap(int capacity, proc* processes)
{
    // Allocating memory to heap h
    heap* h = (heap*)malloc(sizeof(heap));
 
    // Checking if memory is allocated to h or not
    if (h == NULL) {
        printf("Memory error");
        return NULL;
    }
    // set the values to size and capacity
    h->size = 0;
    h->capacity = capacity;
 
    // Allocating memory to array
    h->arr = (int *)malloc(capacity * sizeof(int));
 
    // Checking if memory is allocated to h or not
    if (h->arr == NULL) {
        printf("Memory error");
        return NULL;
    }
    int i;
    for (i = 0; i < capacity; i++) {
        h->arr[i] = i;
    }
 
    h->size = i;
    i = (h->size - 2) / 2;
    while (i >= 0) {
        heapify(h, i);
        i--;
    }
    return h;
}
 
// Defining insertHelper function
void insertHelper(heap* h, int index)
{
    //printf("index: %d\n",index);
    // Store parent of element at index
    // in parent variable
    int parent = (index - 1) / 2;
 
    if (parent != index && !compare(arr[h->arr[parent]],arr[h->arr[index]])) {
        // Swapping when child is smaller
        // than parent element
        int temp = h->arr[parent];
        h->arr[parent] = h->arr[index];
        h->arr[index] = temp;
 
        // Recursively calling insertHelper
        insertHelper(h, parent);
    }
}
 
void heapify(heap* h, int index)
{
    int left = index * 2 + 1;
    int right = index * 2 + 2;
    int min = index;
 
    // Checking whether our left or child element
    // is at right index or not to avoid index error
    if (left >= h->size || left < 0)
        left = -1;
    if (right >= h->size || right < 0)
        right = -1;
 
    // store left or right element in min if
    // any of these is smaller that its parent
    if (left != -1 && compare(arr[h->arr[left]],arr[h->arr[index]]))
        min = left;
    if (right != -1 && compare(arr[h->arr[right]],arr[h->arr[min]]))
        min = right;
 
    // Swapping the nodes
    if (min != index) {
        int temp = h->arr[min];
        h->arr[min] = h->arr[index];
        h->arr[index] = temp;
 
        // recursively calling for their child elements
        // to maintain min heap
        heapify(h, min);
    }
}
 
int extractMin(heap* h)
{
    int deleteItem;
 
    // Checking if the heap is empty or not
    if (h->size == 0) {
        printf("\nHeap id empty.");
        exit(1);
    }
 
    // Store the node in deleteItem that
    // is to be deleted.
    deleteItem = h->arr[0];
 
    // Replace the deleted node with the last node
    h->arr[0] = h->arr[h->size - 1];
    // Decrement the size of heap
    h->size--;
 
    // Call minheapify_top_down for 0th index
    // to maintain the heap property
    heapify(h, 0);
    return deleteItem;
}
 
// Define a insert function
void insert(heap* h, int data)
{
 
    // Checking if heap is full or not
    if (h->size < h->capacity) {
        // Inserting data into an array
        h->arr[h->size] = data;
        // Calling insertHelper function
        insertHelper(h, h->size);
        // Incrementing size of array
        h->size++;
    }
}




proc* readFile(){
    FILE* fptr = fopen(filename,"r");
    char string[100];

    if(fptr == NULL){
        printf("FILE NOT FOUND!\n");
        exit(1);
    }
    fscanf(fptr,"%s",string);
    n = atoi(string);

    proc* arr = (proc *)malloc(sizeof(proc)*(n));
    int pos = 1;
    int num;
    int index = 0;
    while(fscanf(fptr,"%s",string)==1){
        
        num = atoi(string);
        if(pos == 1){
            arr[index].id = num;
            arr[index].completed = 0;
            arr[index].index = 0;
            arr[index].ind = index+1;
            
            pos = 2;
        }
        else if(pos == 2){
            arr[index].arrival_time = arr[index].event_time = num;
            arr[index].size = 0;
            pos = 3;
        }
        else{
            if(num == -1){
                pos = 1;
                index++;
                continue;
            }
            int* size = &arr[index].size;
            arr[index].arr[*size] = num;
            arr[index].done[*size] = 0;
            (*size)++;
        }
    }

    fclose(fptr);
    return arr;
    
}

int top(heap* h){
    if(h==NULL && h->size==0){
        printf("Underflow!\n");
        exit(1);
    }
    int index = h->arr[0];
    return arr[index].event_time;
}
heap* EQ;
queue RQ;

void simulate(int q){
    if(empty(RQ)!=0){
        return;
    }
    int index = front(RQ);

    //printf("front: %d\n",index);
    int ind = arr[index].index;
    int total_time = arr[index].arr[ind], done_time = arr[index].done[ind];
    int rem_time = total_time - done_time;

    //printf("rem time:%d\n",rem_time);

    if(rem_time <= q){
    #ifdef VERBOSE
        fprintf(out,"%-10d: Process %d is scheduled to run for time %d\n",curr_time,index+1,rem_time);
    #endif
        curr_time += rem_time;
        arr[index].done[ind] = arr[index].arr[ind];
        arr[index].index++;

        if(arr[index].index == arr[index].size){
            arr[index].completed = 1;
        }

        while(!Empty(EQ) && curr_time >= top(EQ)){
            int top = extractMin(EQ);
            int status = getStatus(top);

            //printf("top: %d, status:%d\n",top,status);
            if(status == 1 || status == 2){
                if(status == 1){
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue upon arrival\n",arr[top].event_time,top+1);
                #endif
                
                }
                else { 
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue after IO completion\n",arr[top].event_time,top+1);
                #endif

                }
                RQ = enqueue(RQ,top);
                //printf("Done\n");
            }
            else if(status == 5){
                print_status(top);
            }
            else if(status == 4){
                int* index = &arr[top].index;
                arr[top].event_time += arr[top].arr[*index];
                (*index)++;

                
                insert(EQ,top);
            }
            else{
                RQ = enqueue(RQ,top);
            }
        }
        arr[index].event_time = curr_time;

        // int status = getStatus(index);
        // if(status == 4){
        //     arr[index].event_time += arr[index].arr[ind+1];
        // }
        //printf("Done\n");
        insert(EQ,index);

        //printf("Came\n");
        deque(RQ);
    }
    else{
    #ifdef VERBOSE
        fprintf(out,"%-10d: Process %d is scheduled to run for time %d\n",curr_time,index+1,q);
    #endif
        curr_time += q;
        arr[index].done[ind] += q;

        arr[index].event_time = curr_time;

        while(!Empty(EQ) && curr_time >= top(EQ)){
            int top = extractMin(EQ);
            int status = getStatus(top);

            //printf("top: %d, status:%d\n",top,status);
            if(status == 1 || status == 2){
                if(status == 1){
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue upon arrival\n",arr[top].event_time,top+1);
                #endif
                }
                else { 
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue after IO completion\n",arr[top].event_time,top+1);
                #endif
                }
                RQ = enqueue(RQ,top);
                //printf("Done\n");
            }
            else if(status == 5){
                print_status(top);
            }
            else if(status == 4){
                int* index = &arr[top].index;
                arr[top].event_time += arr[top].arr[*index];
                (*index)++;

                
                insert(EQ,top);
            }
            else{
                RQ = enqueue(RQ,top);
            }
        }
        deque(RQ);
    #ifdef VERBOSE
        fprintf(out,"%-10d: Process %d joins ready queue after timeout\n",curr_time,index+1);
    #endif
        enqueue(RQ,index);
    }
    //printf("Done\n");
    return;
}


void schedule(int q){
    arr = readFile();
    wait_time = (int *)malloc(sizeof(int)*n);
    int idle_time = 0;
    EQ = createHeap(n,arr);
    RQ = init();

    if(q == MAX){
        fprintf(out,"**** FCFS Scheduling ****\n");
    }
    else{
        fprintf(out,"**** RR Scheduling with q = %d ****\n",q);
    }
    #ifdef VERBOSE
        fprintf(out,"%-10d: Starting\n",0);
    #endif
    //printf("size:%d\n",EQ->size);
    while(Empty(EQ)==0 || empty(RQ)==0){
        
        if(Empty(EQ)==0 && curr_time >= top(EQ)){  
            int top = extractMin(EQ);
            int status = getStatus(top);

            //printf("top: %d, status:%d\n",top,status);
            if(status == 1 || status == 2){
                if(status == 1){
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue upon arrival\n",arr[top].event_time,top+1);
                #endif
                }
                else  {
                #ifdef VERBOSE
                    fprintf(out,"%-10d: Process %d joins ready queue after IO completion\n",arr[top].event_time,top+1);
                #endif
                }
                RQ = enqueue(RQ,top);
                //printf("Done\n");
            }
            else if(status == 5){
                print_status(top);
            }
            else if(status == 4){
                int* index = &arr[top].index;
                arr[top].event_time += arr[top].arr[*index];
                (*index)++;

                
                insert(EQ,top);
            }
            else{
                RQ = enqueue(RQ,top);
            }
        }
        
        else {
            if(empty(RQ)!=0){
            #ifdef VERBOSE
                fprintf(out,"%-10d: CPU goes idle\n",curr_time);
            #endif
                idle_time += (top(EQ)-curr_time);
                curr_time = top(EQ);
            }
        }

        simulate(q);

    }
    #ifdef VERBOSE
        fprintf(out,"%-10d: CPU goes idle\n",curr_time);
    #endif
    int tat = curr_time;

    double sum = 0;
    for(int i=0;i<n;i++){
        sum += wait_time[i];
    }
    sum /= n;

    fprintf(out,"\nAverage wait time = %.2lf\n",sum);
    fprintf(out,"Total turnaround time = %d\n",tat);
    fprintf(out,"CPU idle time = %d\n",idle_time);
    fprintf(out,"CPU utilization = %.2lf%%\n\n",100.0-(100.0*idle_time)/tat);


    free(arr);
    free(wait_time);
    curr_time = 0;
}



int main(){

    #ifdef VERBOSE
        outfile = "verbose_output1.txt";
    #endif
    out = fopen(outfile,"w");

    #ifdef VERBOSE
        fprintf(out,"gcc -Wall -o schedule -DVERBOSE schedule.c\n");
    #else
        fprintf(out,"gcc -Wall -o schedule schedule.c\n");
    #endif

    fprintf(out,"./schedule\n");

    schedule(MAX);
    schedule(10);
    schedule(5);

    fclose(out);
}