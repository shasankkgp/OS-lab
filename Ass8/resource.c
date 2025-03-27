#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#define MAX_THREADS 100
#define MAX_RESOURCES 20
#define SLEEP_FACTOR 100000  // Factor to scale delay times

/* Resource Tracking Structures */
typedef struct {
    int m;                 // Number of resource types
    int n;                 // Number of threads
    int** NEED;            // Maximum resources minus allocated resources
    int** ALLOC;           // Currently allocated resources to each thread
    int* AVAILABLE;        // Available resources in the system
    int** MAX;             // Maximum resources each thread can claim
} ResourceManager;

/* Resource Request Structure */
typedef struct {
    int* REQ;              // Request vector for resources
    int thread_id;         // Thread making the request
    int type;              // 0=RELEASE, 1=ADDITIONAL, 2=QUIT
} Request;

/* Request Queue Implementation */
typedef struct {
    int front, rear, size;
    int capacity;
    int* threads;          // Thread IDs in queue
} Queue;

/* Thread Status Tracking */
typedef struct {
    int* active;           // Track which threads are still active
    int active_count;      // Number of active threads
    int* waiting;          // Track which threads are waiting
} ThreadTracker;

/* synhronization Tools */
typedef struct {
    pthread_barrier_t BOS;            // Beginning of session barrier
    pthread_barrier_t REQB;           // Request barrier
    pthread_barrier_t* ACKB;          // Acknowledgment barriers (per thread)
    pthread_cond_t* cv;               // Condition variables (per thread)
    pthread_mutex_t* cmtx;            // Condition variable mutexes (per thread)
    pthread_mutex_t rmtx;             // Resource request mutex
    pthread_mutex_t pmtx;             // Print mutex
} synTools;

/* Global variables */
ResourceManager rm;
Request req;
ThreadTracker tt;
synTools syn;

/* Function Prototypes */
void init_resource_manager(int m, int n);
void init_syn_tools(int n);
bool is_safe(int thread_id, int* request);
void process_request(Request* request, Queue* Q, int** request_matrix);
void grant_pending_requests(Queue* Q, int** request_matrix);
void cleanup();
void print_waiting_threads(Queue* Q);
void* thread_function(void* arg);

/* Queue Management Functions */
Queue* create_queue(int capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = -1;
    queue->threads = (int*)malloc(capacity * sizeof(int));
    return queue;
}

bool is_empty(Queue* queue) {
    return (queue->size == 0);
}

bool is_full(Queue* queue) {
    return (queue->size == queue->capacity);
}

void enqueue(Queue* queue, int thread_id) {
    if (is_full(queue)) {
        printf("Warning: Queue is full\n");
        return;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->threads[queue->rear] = thread_id;
    queue->size++;
}

int dequeue(Queue* queue) {
    if (is_empty(queue)) {
        printf("Warning: Queue is empty\n");
        return -1;
    }
    int thread_id = queue->threads[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return thread_id;
}

/* Initialize Resource Manager */
void init_resource_manager(int m, int n) {
    rm.m = m;
    rm.n = n;
    
    // Allocate memory for resource tracking matrices
    rm.NEED = (int**)malloc(MAX_THREADS * sizeof(int*));
    rm.ALLOC = (int**)malloc(MAX_THREADS * sizeof(int*));
    rm.MAX = (int**)malloc(MAX_THREADS * sizeof(int*));
    rm.AVAILABLE = (int*)malloc(MAX_RESOURCES * sizeof(int));
    
    // Initialize allocated memory
    for (int i = 0; i < MAX_THREADS; i++) {
        rm.NEED[i] = (int*)malloc(MAX_RESOURCES * sizeof(int));
        rm.ALLOC[i] = (int*)malloc(MAX_RESOURCES * sizeof(int));
        rm.MAX[i] = (int*)malloc(MAX_RESOURCES * sizeof(int));
        
        for (int j = 0; j < MAX_RESOURCES; j++) {
            rm.MAX[i][j] = 0;
            rm.ALLOC[i][j] = 0;
            rm.NEED[i][j] = 0;
        }
    }
    
    // Initialize available resources
    for (int i = 0; i < MAX_RESOURCES; i++) {
        rm.AVAILABLE[i] = 0;
    }
}

/* Initialize Thread Status Tracking */
void init_thread_tracker(int n) {
    tt.active = (int*)malloc(n * sizeof(int));
    tt.waiting = (int*)malloc(n * sizeof(int));
    tt.active_count = n;
    
    for (int i = 0; i < n; i++) {
        tt.active[i] = 1;
        tt.waiting[i] = 0;
    }
}

/* Initialize synhronization Tools */
void init_syn_tools(int n) {
    // Initialize mutex for resource requests
    pthread_mutex_init(&syn.rmtx, NULL);
    pthread_mutex_init(&syn.pmtx, NULL);
    
    // Initialize barriers
    pthread_barrier_init(&syn.BOS, NULL, n + 1); // All threads + master
    pthread_barrier_init(&syn.REQB, NULL, 2);    // Master + requesting thread
    
    // Initialize thread-specific synhronization tools
    syn.ACKB = (pthread_barrier_t*)malloc(n * sizeof(pthread_barrier_t));
    syn.cv = (pthread_cond_t*)malloc(n * sizeof(pthread_cond_t));
    syn.cmtx = (pthread_mutex_t*)malloc(n * sizeof(pthread_mutex_t));
    
    for (int i = 0; i < n; i++) {
        pthread_barrier_init(&syn.ACKB[i], NULL, 2);  // Master + thread i
        pthread_cond_init(&syn.cv[i], NULL);
        pthread_mutex_init(&syn.cmtx[i], NULL);
    }
}

/* Banker's Algorithm: Check if state is safe after allocation */
bool is_safe(int thread_id, int* request) {
    // First check if request exceeds need or available resources
    for (int i = 0; i < rm.m; i++) {
        if (request[i] > rm.NEED[thread_id][i]) {
            pthread_mutex_lock(&syn.pmtx);
            printf("\t+++ Request exceeds stated maximum for thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
            return false;
        }
        
        if (request[i] > rm.AVAILABLE[i]) {
            pthread_mutex_lock(&syn.pmtx);
            printf("\t+++ Insufficient resources to grant request of thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
            return false;
        }
    }
    
#ifdef _DLAVOID
    // Safety check (Banker's Algorithm)
    int finish[rm.n];
    int work[rm.m];
    int temp_alloc[MAX_THREADS][MAX_RESOURCES];
    int temp_need[MAX_THREADS][MAX_RESOURCES];
    
    // Copy current state
    for (int i = 0; i < rm.m; i++) {
        work[i] = rm.AVAILABLE[i] - request[i]; // Simulate allocation
    }
    
    for (int i = 0; i < rm.n; i++) {
        finish[i] = 0;
        for (int j = 0; j < rm.m; j++) {
            temp_alloc[i][j] = rm.ALLOC[i][j];
            temp_need[i][j] = rm.NEED[i][j];
        }
    }

    // Simulate allocation for requesting thread
    for (int i = 0; i < rm.m; i++) {
        temp_alloc[thread_id][i] += request[i];
        temp_need[thread_id][i] -= request[i];
    }
    
    // Find a safe sequence
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < rm.n; i++) {
            if (!finish[i]) {
                bool can_finish = true;
                
                // Check if thread can get all needed resources
                for (int j = 0; j < rm.m; j++) {
                    if (temp_need[i][j] > work[j]) {
                        can_finish = false;
                        break;
                    }
                }
                
                if (can_finish) {
                    // Thread can finish - add its resources to work
                    finish[i] = 1;
                    changed = true;
                    
                    for (int j = 0; j < rm.m; j++) {
                        work[j] += temp_alloc[i][j];
                    }
                }
            }
        }
    } while (changed);
    
    // Check if all threads can finish
    for (int i = 0; i < rm.n; i++) {
        if (!finish[i] && tt.active[i]) {
            pthread_mutex_lock(&syn.pmtx);
            printf("\t+++ Unsafe state if request of thread %d is granted\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
            return false;
        }
    }
#endif
    return true;
}

/* Process a resource request */
void process_request(Request* request, Queue* Q, int** request_matrix) {
    int thread_id = request->thread_id;
    
    if (request->type == 1) {  // ADDITIONAL request
        // Handle negative entries (releases) first
        for (int i = 0; i < rm.m; i++) {
            if (request->REQ[i] < 0) {
                rm.ALLOC[thread_id][i] += request->REQ[i];  // Subtract released resources
                rm.NEED[thread_id][i] -= request->REQ[i];   // Add back to needed resources
                rm.AVAILABLE[i] -= request->REQ[i];         // Add to available pool
                request->REQ[i] = 0;                       // Clear the release part
            }
            request_matrix[thread_id][i] = request->REQ[i]; // Store remaining request
        }
        
        // Enqueue the request
        enqueue(Q, thread_id);
        
        pthread_mutex_lock(&syn.pmtx);
        printf("Master thread stores resource request of thread %d\n", thread_id);
        print_waiting_threads(Q);
        fflush(stdout);
        pthread_mutex_unlock(&syn.pmtx);
    } else {  // RELEASE or QUIT request
        // Process releases (negative values in REQ)
        for (int i = 0; i < rm.m; i++) {
            rm.ALLOC[thread_id][i] += request->REQ[i];   // Subtract from allocations
            rm.NEED[thread_id][i] -= request->REQ[i];    // Add back to needs
            rm.AVAILABLE[i] -= request->REQ[i];          // Add to available pool
        }
        
        // If it's a QUIT request, mark thread as inactive
        if (request->type == 2) {
            tt.active[thread_id] = 0;
            tt.active_count--;
            
            pthread_mutex_lock(&syn.pmtx);
            printf("Master thread releases resources of thread %d\n", thread_id);
            print_waiting_threads(Q);
            
            printf("%d threads left:", tt.active_count);
            for (int i = 0; i < rm.n; i++) {
                if (tt.active[i]) {
                    printf(" %d", i);
                }
            }
            printf("\n");
            
            printf("Available resources:");
            for (int i = 0; i < rm.m; i++) {
                printf(" %d", rm.AVAILABLE[i]);
            }
            printf("\n");
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
        }
    }
    
    // Reset request data
    for (int i = 0; i < rm.m; i++) {
        request->REQ[i] = 0;
    }
}

/* Try to grant pending requests */
void grant_pending_requests(Queue* Q, int** request_matrix) {
    pthread_mutex_lock(&syn.pmtx);
    printf("Master thread tries to grant pending requests\n");
    fflush(stdout);
    pthread_mutex_unlock(&syn.pmtx);
    
    int queue_size = Q->size;
    for (int i = 0; i < queue_size; i++) {
        int thread_id = dequeue(Q);
        
        if (is_safe(thread_id, request_matrix[thread_id])) {
            // Grant request
            for (int j = 0; j < rm.m; j++) {
                rm.AVAILABLE[j] -= request_matrix[thread_id][j];
                rm.ALLOC[thread_id][j] += request_matrix[thread_id][j];
                rm.NEED[thread_id][j] -= request_matrix[thread_id][j];
                request_matrix[thread_id][j] = 0;
            }
            
            pthread_mutex_lock(&syn.pmtx);
            printf("Master thread grants resource request for thread %d\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
            
            // Wait until thread is waiting before signaling
            while (tt.waiting[thread_id] == 0) {
                // Spin wait
            }
            
            // Signal thread that its request is granted
            pthread_mutex_lock(&syn.cmtx[thread_id]);
            pthread_cond_signal(&syn.cv[thread_id]);
            pthread_mutex_unlock(&syn.cmtx[thread_id]);
        } else {
            // Return request to queue
            enqueue(Q, thread_id);
        }
    }
    
    pthread_mutex_lock(&syn.pmtx);
    print_waiting_threads(Q);
    fflush(stdout);
    pthread_mutex_unlock(&syn.pmtx);
}

/* Display waiting threads */
void print_waiting_threads(Queue* Q) {
    printf("\t\tWaiting threads:");
    int queue_size = Q->size;
    
    // Temporary storage for thread IDs
    int* temp_ids = (int*)malloc(queue_size * sizeof(int));
    
    // Dequeue and store all IDs
    for (int i = 0; i < queue_size; i++) {
        temp_ids[i] = dequeue(Q);
        printf(" %d", temp_ids[i]);
    }
    
    // Re-enqueue all IDs in same order
    for (int i = 0; i < queue_size; i++) {
        enqueue(Q, temp_ids[i]);
    }
    
    free(temp_ids);
    printf("\n");
}

/* Cleanup allocated resources */
void cleanup() {
    pthread_mutex_destroy(&syn.rmtx);
    pthread_mutex_destroy(&syn.pmtx);
    
    pthread_barrier_destroy(&syn.BOS);
    pthread_barrier_destroy(&syn.REQB);
    
    for (int i = 0; i < rm.n; i++) {
        pthread_cond_destroy(&syn.cv[i]);
        pthread_mutex_destroy(&syn.cmtx[i]);
        pthread_barrier_destroy(&syn.ACKB[i]);
    }
    
    for (int i = 0; i < MAX_THREADS; i++) {
        free(rm.MAX[i]);
        free(rm.ALLOC[i]);
        free(rm.NEED[i]);
    }
    
    free(rm.MAX);
    free(rm.ALLOC);
    free(rm.NEED);
    free(rm.AVAILABLE);
    
    free(syn.ACKB);
    free(syn.cv);
    free(syn.cmtx);
    
    free(tt.active);
    free(tt.waiting);
}

/* Thread function implementation */
void* thread_function(void* arg) {
    int* index = (int*)arg;
    int thread_id = *index;
    pthread_mutex_lock(&syn.pmtx);
    printf("\tThread %d born\n", thread_id);
    fflush(stdout);
    pthread_mutex_unlock(&syn.pmtx);
    
    char request_type;
    int DELAY = 0;
    int resource_holdings[MAX_RESOURCES];
    int request_values[MAX_RESOURCES];
    bool is_quitting = false;
    
    // Initialize resource holdings
    for (int i = 0; i < rm.m; i++) {
        rm.MAX[thread_id][i] = 0;
        resource_holdings[i] = 0;
    }
    
    // Open input file
    char filename[30];
    sprintf(filename, "input/thread%02d.txt", thread_id);
    
    FILE* input_file = fopen(filename, "r");
    if (input_file == NULL) {
        pthread_mutex_lock(&syn.pmtx);
        printf("Error opening file: %s\n", filename);
        fflush(stdout);
        pthread_mutex_unlock(&syn.pmtx);
        pthread_exit(NULL);
    }
    
    // Read maximum needs
    for (int i = 0; i < rm.m; i++) {
        fscanf(input_file, "%d", &rm.MAX[thread_id][i]);
        rm.NEED[thread_id][i] = rm.MAX[thread_id][i];
    }
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&syn.BOS);
    
    // Process resource requests
    while (1) {
        fscanf(input_file, "%d %c", &DELAY, &request_type);
        
        if (request_type == 'R') {
            // Read resource request values
            for (int i = 0; i < rm.m; i++) {
                fscanf(input_file, " %d", &request_values[i]);
            }
        } else if (request_type == 'Q') {
            // Prepare quit request (release all resources)
            for (int i = 0; i < rm.m; i++) {
                request_values[i] = -resource_holdings[i];
            }
            is_quitting = true;
        }
        
        // Determine request type
        int request_category = 0; // Default: RELEASE
        for (int i = 0; i < rm.m; i++) {
            if (request_values[i] > 0) {
                request_category = 1; // ADDITIONAL
                break;
            }
        }
        
        if (is_quitting) {
            request_category = 2; // QUIT
        }
        
        // Simulate work
        usleep(DELAY * SLEEP_FACTOR);
        
        // Submit resource request
        pthread_mutex_lock(&syn.rmtx);
        req.type = request_category;
        req.thread_id = thread_id;
        
        for (int i = 0; i < rm.m; i++) {
            req.REQ[i] = request_values[i];
        }
        
        pthread_mutex_lock(&syn.pmtx);
        if (request_category == 1) {
            printf("\tThread %d sends resource request: type = ADDITIONAL\n", thread_id);
        } else if (request_category == 0) {
            printf("\tThread %d sends resource request: type = RELEASE\n", thread_id);
        } else {
            printf("\tThread %d sends resource request: type = QUIT\n", thread_id);
        }
        fflush(stdout);
        pthread_mutex_unlock(&syn.pmtx);
        
        // synhronize with master thread
        pthread_barrier_wait(&syn.REQB);
        pthread_barrier_wait(&syn.ACKB[thread_id]);
        pthread_mutex_unlock(&syn.rmtx);
        
        if (request_category == 1) { // ADDITIONAL request
            // Wait for request to be granted
            pthread_mutex_lock(&syn.cmtx[thread_id]);
            tt.waiting[thread_id] = 1;
            pthread_cond_wait(&syn.cv[thread_id], &syn.cmtx[thread_id]);
            tt.waiting[thread_id] = 0;
            pthread_mutex_unlock(&syn.cmtx[thread_id]);
            
            pthread_mutex_lock(&syn.pmtx);
            printf("\tThread %d is granted its last resource request\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
        } else if (request_category == 0) { // RELEASE request
            pthread_mutex_lock(&syn.pmtx);
            printf("\tThread %d is done with its resource release request\n", thread_id);
            fflush(stdout);
            pthread_mutex_unlock(&syn.pmtx);
        }
        
        // Update local resource holdings
        for (int i = 0; i < rm.m; i++) {
            resource_holdings[i] += request_values[i];
            request_values[i] = 0; // Reset for next request
        }
        
        if (is_quitting) {
            break;
        }
    }
    
    pthread_mutex_lock(&syn.pmtx);
    printf("\tThread %d going to quit\n", thread_id);
    fflush(stdout);
    pthread_mutex_unlock(&syn.pmtx);
    
    fclose(input_file);
    pthread_exit(NULL);
}

/* Main function */
int main() {
    // Initialize resource request structure
    req.REQ = (int*)malloc(MAX_RESOURCES * sizeof(int));
    for (int i = 0; i < MAX_RESOURCES; i++) {
        req.REQ[i] = 0;
    }
    
    // Read system configuration
    FILE* system_file = fopen("input/system.txt", "r");
    if (system_file == NULL) {
        printf("Failed to read system.txt file\n");
        exit(1);
    }
    
    int m, n;
    fscanf(system_file, "%d", &m);
    if (m < 5 || m > 20) {
        printf("Invalid value for number of resources\n");
        fclose(system_file);
        exit(1);
    }
    
    fscanf(system_file, "%d", &n);
    if (n < 10 || n > 100) {
        printf("Invalid value for number of threads\n");
        fclose(system_file);
        exit(1);
    }
    
    // Initialize resource manager
    init_resource_manager(m, n);
    
    // Read available resource counts
    for (int i = 0; i < m; i++) {
        fscanf(system_file, "%d", &rm.AVAILABLE[i]);
    }
    fclose(system_file);
    
    // Initialize thread tracker
    init_thread_tracker(n);
    
    // Initialize synhronization tools
    init_syn_tools(n);
    
    // Create request queue
    Queue* Q = create_queue(MAX_THREADS);
    
    // Create user threads
    pthread_t* threads = (pthread_t*)malloc(n * sizeof(pthread_t));
    int* thread_ids = (int*)malloc(n * sizeof(int));
    
    for (int i = 0; i < n; i++) {
        thread_ids[i] = i;
    }
    
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, thread_function, (void*)(thread_ids + i));
    }
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&syn.BOS);
    
    // Create request matrix to store pending requests
    int** request_matrix = (int**)malloc(MAX_THREADS * sizeof(int*));
    for (int i = 0; i < MAX_THREADS; i++) {
        request_matrix[i] = (int*)malloc(MAX_RESOURCES * sizeof(int));
        memset(request_matrix[i], 0, MAX_RESOURCES * sizeof(int));
    }
    
    // Main processing loop - master thread acts as OS
    while (1) {
        // Wait for a request from any thread
        pthread_barrier_wait(&syn.REQB);
        
        // Process the request
        process_request(&req, Q, request_matrix);
        
        // Acknowledge request receipt
        pthread_barrier_wait(&syn.ACKB[req.thread_id]);
        
        // Exit if all threads have terminated
        if (tt.active_count == 0) {
            break;
        }
        
        // Try to grant pending requests
        grant_pending_requests(Q, request_matrix);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up
    free(req.REQ);
    free(threads);
    free(thread_ids);
    
    for (int i = 0; i < MAX_THREADS; i++) {
        free(request_matrix[i]);
    }
    free(request_matrix);
    
    free(Q->threads);
    free(Q);
    
    cleanup();
    
    return 0;
}