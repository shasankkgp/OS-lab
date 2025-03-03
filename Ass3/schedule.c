#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<limits.h>

#define MAX 100000



struct process{
    int pid;
    int arrival_time;
    int num_bursts;
    int rbt;
    int index;
    int status;                  // new=2 , timeout=0 , io complete=1 , exit=-2, io_start = -1
    int turnaround_time;
    int waiting_time;
    int cpu_bursts[20];
    int io_bursts[20];
}process;

typedef struct process* processes;

processes* process_info;

typedef struct {
    int items[MAX];
    int front;
    int rear;
} Queue;

void init(Queue *q) {
    q->front = -1;
    q->rear = -1;
}

int isEmpty(Queue *q) {
    return q->front == -1;
}

int isFull(Queue *q) {
    return q->rear == MAX - 1;
}

int front(Queue *q) {
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return -1;
    }
    return q->items[q->front];
}

void enqueue(Queue *q, int value) {
    // printf("EnQueue : %d  ",value);
    if (isFull(q)) {
        printf("Queue is full\n");
        return;
    }
    if (isEmpty(q)) {
        q->front = 0;
    }
    q->rear++;
    q->items[q->rear] = value;
}

int dequeue(Queue *q) {
    if (isEmpty(q)) {
        printf("Queue is empty\n");
        return -1;
    }
    int item = q->items[q->front];
    q->front++;
    if (q->front > q->rear) {
        q->front = q->rear = -1;
    }
    // printf("DeQueue : %d  ",item);
    return item;
}


typedef struct {
    int data[MAX];
    int id[MAX];
    int size;
} MinHeap;

void init_(MinHeap *heap) {
    heap->size = 0;
}

int parent(int i) {
    return (i - 1) / 2;
}

int left(int i) {
    return (2 * i + 1);
}

int right(int i) {
    return (2 * i + 2);
}

void swap(int *x, int *y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

void insertKey(MinHeap *heap, int key, int id) {
    if (heap->size == MAX) {
        printf("Overflow: Could not insertKey\n");
        return;
    }

    heap->size++;
    int i = heap->size - 1;
    heap->data[i] = key;
    heap->id[i] = id;

    while (i != 0 && ((heap->data[parent(i)] > heap->data[i]) || 
        ((heap->data[parent(i)] == heap->data[i]) && 
        (((process_info[heap->id[parent(i)]]->status>0) < (process_info[heap->id[i]]->status>0)) || 
        (((process_info[heap->id[parent(i)]]->status>0) == (process_info[heap->id[i]]->status>0)) && heap->id[parent(i)] > heap->id[i]))))) {
        swap(&heap->data[i], &heap->data[parent(i)]);
        swap(&heap->id[i], &heap->id[parent(i)]);
        i = parent(i);
    }
}

void decreaseKey(MinHeap *heap, int i, int new_val, int id) {
    heap->data[i] = new_val;
    heap->id[i] = id;
    while (i != 0 && (heap->data[parent(i)] > heap->data[i] || 
        (heap->data[parent(i)] == heap->data[i] && 
        (process_info[heap->id[parent(i)]]->status < process_info[heap->id[i]]->status || 
        (process_info[heap->id[parent(i)]]->status == process_info[heap->id[i]]->status && heap->id[parent(i)] > heap->id[i]))))) {
        swap(&heap->data[i], &heap->data[parent(i)]);
        swap(&heap->id[i], &heap->id[parent(i)]);
        i = parent(i);
    }
}

void minHeapify(MinHeap *heap, int i) {
    int l = left(i);
    int r = right(i);
    int smallest = i;

    if (l <= heap->size && (heap->data[l] < heap->data[smallest] || 
        (heap->data[l] == heap->data[smallest] && 
        ((process_info[heap->id[l]]->status>0) > (process_info[heap->id[smallest]]->status>0) || 
        ((process_info[heap->id[l]]->status>0) == (process_info[heap->id[smallest]]->status>0) && heap->id[l] < heap->id[smallest])))))
        smallest = l;

    if (r <= heap->size && (heap->data[r] < heap->data[smallest] || 
        (heap->data[r] == heap->data[smallest] && 
        ((process_info[heap->id[r]]->status>0) > (process_info[heap->id[smallest]]->status>0) || 
        ((process_info[heap->id[r]]->status>0) == (process_info[heap->id[smallest]]->status>0) && heap->id[r] < heap->id[smallest])))))
        smallest = r;

    if (smallest != i) {
        swap(&heap->data[i], &heap->data[smallest]);
        swap(&heap->id[i], &heap->id[smallest]);
        minHeapify(heap, smallest);
    }
}

int* extractMin(MinHeap *heap) {
    if (heap->size <= 0)
        return NULL; // Return NULL instead of INT_MAX

    if (heap->size == 1) {
        heap->size--;
        int* ans = (int*)malloc(2 * sizeof(int));
        ans[0] = heap->data[0];
        ans[1] = heap->id[0];
        return ans; // Return pointer to the data
    }

    int* ans = (int*)malloc(2 * sizeof(int));
    ans[0] = heap->data[0];
    ans[1] = heap->id[0];
    heap->data[0] = heap->data[heap->size - 1];
    heap->id[0] = heap->id[heap->size - 1];
    heap->size--;
    minHeapify(heap, 0);

    return ans;
}

int getMin(MinHeap *heap) {
    return heap->data[0];
}


void scheduler(int q, MinHeap* event_queue, Queue* ready_queue) {
    printf("0       : Starting\n");
    int current_time = 0;
    int total_turnaround_time = 0;
    int total_waiting_time = 0;
    int cpu_idle_time = 0;
    int total_processes = 0;
    int time = 0;

    while (event_queue->size || isEmpty(ready_queue) == 0)
    {
        // printf("Hello World : %d \n ",event_queue->size);
        fflush(stdout);
        if (current_time <= time && isEmpty(ready_queue) == 0) {
            // printf("Whyyyy\n");
            // fflush(stdout);

            cpu_idle_time += (time - current_time);

            int pid=dequeue(ready_queue);    // get the process from the ready queue

            if (process_info[pid]->rbt <= q)
            {  // if the process can complete in the time quantum

                if (process_info[pid]->index < process_info[pid]->num_bursts - 1)
                {  // not a last process
                    // printf("Shahsh 1 %d : : " ,current_time + process_info[pid]->rbt + process_info[pid]->io_bursts[process_info[pid]->index]);
                    
                    process_info[pid]->status = -1;
                    insertKey(event_queue, time + process_info[pid]->rbt, pid);
                    // if (pid == 16 && time == ) 
                    // enqueue(ready_queue, pid);
                    printf("%d\t: Process %d is scheduled to run for time %d\n", time, pid, process_info[pid]->rbt);
                    current_time = time + process_info[pid]->rbt;
                    process_info[pid]->rbt = process_info[pid]->cpu_bursts[process_info[pid]->index + 1];
                    process_info[pid]->index++;
                } 
                else if (process_info[pid]->index == process_info[pid]->num_bursts - 1)
                {  // last process so exit
                    // printf("Shahsh 2 ");
                    process_info[pid]->status = -2;
                    printf("%d\t: Process %d is scheduled to run for time %d\n", time, pid, process_info[pid]->rbt);
                    fflush(stdout);
                    insertKey(event_queue, time + process_info[pid]->rbt, pid);
                    current_time = time + process_info[pid]->rbt;
                }
            } else {    // process can't complete in the time quantum so time out interrupt 
                    // printf("Shahsh 3 ");

                printf("%d\t: Process %d is scheduled to run for time %d\n", time, pid, q);
                process_info[pid]->rbt -= q;
                process_info[pid]->status = 0;
                insertKey(event_queue, time + q, pid);
                // enqueue(ready_queue, pid);
                current_time = time + q;
            }

            continue;
        }

        int *event = extractMin(event_queue);
// 
        // printf("Hello World : %d \n ",event[0]);
        fflush(stdout);
        time = event[0];
        int pid = event[1];
        if (current_time < time) printf("%d\t: CPU goes idle %d %d\n", current_time, pid, time);

        // if (current_time > time) 
        // {        // the event is in past so reinsert it
        //     if (process_info[pid]->status == 2) {   // new process
        //         printf("%d\t: Process %d joins the ready queue upon arrival\n", time, pid);
        //         enqueue(ready_queue, pid);
        //     } else if (process_info[pid]->status == 0) {  // process came from timeout
        //         printf("%d\t: Process %d joins ready queue after timeout\n", time, pid);
        //         enqueue(ready_queue, pid);
        //     } else if (process_info[pid]->status == 1) {   // process came from IO completion
        //         printf("%d\t: Process %d joins ready queue after IO completion\n", time, pid);
        //         enqueue(ready_queue, pid);
        //     }
        //     else if (process_info[pid]->status == 3) {   // process came from IO completion
        //         printf("%d\t: Process %d exit\n", time, pid);
        //         continue;            
        //     }
        //     process_info[pid]->status = 5;
        //     insertKey(event_queue, current_time, pid);
        //     continue;
        // }

        if (process_info[pid]->status == 2) {   // new process
            printf("%d\t: Process %d joins the ready queue upon arrival\n", time, pid);
            enqueue(ready_queue, pid);
        } else if (process_info[pid]->status == 0) {  // process came from timeout
            printf("%d\t: Process %d joins ready queue after timeout\n", time, pid);
            enqueue(ready_queue, pid);
        } else if (process_info[pid]->status == 1) {   // process came from IO completion
            printf("%d\t: Process %d joins ready queue after IO completion\n", time, pid);
            enqueue(ready_queue, pid);
        }
        else if (process_info[pid]->status == -2) {   // process came from IO completion
            printf("%d\t: Process %d exit\n", time, pid);
            continue;
        }else if(process_info[pid]->status == 5){
            printf("%d\t: Process %d is scheduled to run for time %d\n", time, pid, process_info[pid]->rbt);
            process_info[pid]->status = 1;
        } else if (process_info[pid]->status == -1) {
            process_info[pid]->status = 1;
            insertKey(event_queue, time + process_info[pid]->io_bursts[process_info[pid]->index-1], pid);
            // printf("%d\t: Process %d is scheduled to run for time %d\n", time, pid, process_info[pid]->rbt);
        }
        // fflush(stdout);
    }

    while (event_queue->size) {
        extractMin(event_queue);
    }

    printf("Average wait time = %.2f\n", (float)total_waiting_time / total_processes);
    printf("Total turnaround time = %d\n", total_turnaround_time);
    printf("CPU idle time = %d\n", cpu_idle_time);
    printf("CPU utilization = %.2f%%\n", 100.0 * (current_time - cpu_idle_time) / current_time);
}


void scheduler_(int q, MinHeap* event_queue, Queue* ready_queue) {
    // printf("0       : Starting\n");
    enqueue(ready_queue, 1);
    int current_time = 0;
    int total_turnaround_time = 0;
    int total_waiting_time = 0;
    int cpu_idle_time = 0;
    int total_processes = 0;

    while (event_queue->size) {
        int *event = extractMin(event_queue);
        int time = event[0];
        int pid = event[1];

        if (current_time < time) {
            cpu_idle_time += (time - current_time);
            // printf("%d\t: CPU goes idle\n", current_time);
            current_time = time;
        }

        if (process_info[pid]->status == 2) {
            // printf("%d\t: Process %d joins the ready queue upon arrival\n", time, pid);
        } else if (process_info[pid]->status == 0) {
            // printf("%d\t: Process %d joins ready queue after timeout\n", time, pid);
        } else if (process_info[pid]->status == 1) {
            // printf("%d\t: Process %d joins ready queue after IO completion\n", time, pid);
        }

        if (current_time > time) {
            insertKey(event_queue, current_time, pid);
            continue;
        }

        current_time = time;

        if (isEmpty(ready_queue)) {
            break;
        }

        if (process_info[pid]->rbt <= q) {
            if (process_info[pid]->index < process_info[pid]->num_bursts - 1) {
                insertKey(event_queue, current_time + process_info[pid]->rbt + process_info[pid]->io_bursts[process_info[pid]->index], pid);
                enqueue(ready_queue, pid);
                // printf("%d\t: Process %d is scheduled to run for time %d\n", current_time, pid, process_info[pid]->rbt);
                current_time += process_info[pid]->rbt;
                process_info[pid]->rbt = process_info[pid]->cpu_bursts[process_info[pid]->index + 1];
                process_info[pid]->index++;
                process_info[pid]->status = 1;
            } else if (process_info[pid]->index == process_info[pid]->num_bursts - 1) {
                // printf("%d\t: Process %d is scheduled to run for time %d\n", current_time, pid, process_info[pid]->rbt);
                int completion_time = current_time + process_info[pid]->rbt;
                int turnaround_time = completion_time - process_info[pid]->arrival_time;
                int waiting_time = turnaround_time - process_info[pid]->cpu_bursts[process_info[pid]->index];
                printf("%d\t: Process %d exits. Turnaround time = %d, Wait time = %d\n", completion_time, pid, turnaround_time, waiting_time);
                total_turnaround_time += turnaround_time;
                total_waiting_time += waiting_time;
                total_processes++;
                current_time += process_info[pid]->rbt;
            }
        } else {
            // printf("%d\t: Process %d is scheduled to run for time %d\n", current_time, pid, q);
            process_info[pid]->rbt -= q;
            process_info[pid]->status = 0;
            insertKey(event_queue, current_time + q, pid);
            enqueue(ready_queue, pid);
            current_time += q;
        }
    }

    while (event_queue->size) {
        extractMin(event_queue);
    }

    printf("Average wait time = %.2f\n", (float)total_waiting_time / total_processes);
    printf("Total turnaround time = %d\n", total_turnaround_time);
    printf("CPU idle time = %d\n", cpu_idle_time);
    printf("CPU utilization = %.2f%%\n", 100.0 * (current_time - cpu_idle_time) / current_time);
}




int main(){

    // Part 1

    FILE* fp=fopen("proc.txt","r");
    if(fp == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    int num_processes;
    fscanf(fp,"%d",&num_processes);
    processes p = (processes)malloc(num_processes * sizeof(struct process));

    process_info = (processes*)malloc((num_processes+1) * sizeof(struct process*));

    for(int i = 0; i < num_processes; i++) {
        fscanf(fp,"%d %d",&p[i].pid,&p[i].arrival_time);
        int num_bursts=0;
        do{
            fscanf(fp,"%d",&p[i].cpu_bursts[num_bursts]);
            fscanf(fp,"%d",&p[i].io_bursts[num_bursts]);
            num_bursts++;
        }while(p[i].io_bursts[num_bursts-1]!=-1);
        p[i].io_bursts[num_bursts-1]=0;
        p[i].num_bursts=num_bursts;
    }

    for( int i=0 ; i<num_processes ; i++ ){
        p[i].rbt= p[i].cpu_bursts[0];
        p[i].status = 2;  // status = 2 means new process
        p[i].index = 0;
    }

    for( int i=0 ; i<num_processes ; i++){
        process_info[p[i].pid] = &p[i];
    }

    fclose(fp);

    // Part 2 

    Queue* ready_queue = (Queue*)malloc(sizeof(Queue));
    init(ready_queue);

    // Part 3

    MinHeap* event_queue = (MinHeap*)malloc(sizeof(MinHeap));
    init_(event_queue);

    for( int i=0 ; i<num_processes ; i++ ){
        insertKey(event_queue,p[i].arrival_time,p[i].pid);
    }
    // insertKey(event_queue,150,10);


    // printf("printing minheap\n");
    // for( int i=0 ; i<event_queue->size ; i++ ){
    //     printf("%d %d\n",event_queue->data[i],event_queue->id[i]);
    // }
    #ifdef VERBOSE

    printf("\n**** FCFS Scheduling ****\n");

    scheduler(1000000,event_queue,ready_queue);

    for( int i=0 ; i<num_processes ; i++ ){
        p[i].rbt= p[i].cpu_bursts[0];
        p[i].status = 2;  // status = 2 means new process
        p[i].index = 0;
    }

    for( int i=0 ; i<num_processes ; i++ ){
        insertKey(event_queue,p[i].arrival_time,p[i].pid);
    }

    printf("\n**** RR Scheduling with q = 10 ****\n");

    scheduler(10,event_queue,ready_queue);

    for( int i=0 ; i<num_processes ; i++ ){
        p[i].rbt= p[i].cpu_bursts[0];
        p[i].status = 2;  // status = 2 means new process
        p[i].index = 0;
    }

    for( int i=0 ; i<num_processes ; i++ ){
        insertKey(event_queue,p[i].arrival_time,p[i].pid);
    }

    printf("\n**** RR Scheduling with q = 5 ****\n");

    scheduler(5,event_queue,ready_queue);

    #endif

    #ifndef VERBOSE

    printf("**** FCFS Scheduling ****\n");

    scheduler_(1000000,event_queue,ready_queue);

    for( int i=0 ; i<num_processes ; i++ ){
        p[i].rbt= p[i].cpu_bursts[0];
        p[i].status = 2;  // status = 2 means new process
        p[i].index = 0;
    }

    for( int i=0 ; i<num_processes ; i++ ){
        insertKey(event_queue,p[i].arrival_time,p[i].pid);
    }

    printf("\n**** RR Scheduling with q = 10 ****\n");

    scheduler_(10,event_queue,ready_queue);

    for( int i=0 ; i<num_processes ; i++ ){
        p[i].rbt= p[i].cpu_bursts[0];
        p[i].status = 2;  // status = 2 means new process
        p[i].index = 0;
    }

    for( int i=0 ; i<num_processes ; i++ ){
        insertKey(event_queue,p[i].arrival_time,p[i].pid);
    }

    printf("\n**** RR Scheduling with q = 5 ****\n");

    scheduler_(5,event_queue,ready_queue);

    #endif

    free(p);
    return 0;
}