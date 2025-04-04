#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Constants 
#define PAGE_SIZE 4096 // 4 KB
#define TOTAL_MEMORY (64 * 1024 * 1024) // 64 MB
#define OS_MEMORY (16 * 1024 * 1024) // 16 MB
#define USER_MEMORY (TOTAL_MEMORY - OS_MEMORY) // 48 MB
#define TOTAL_FRAMES (TOTAL_MEMORY / PAGE_SIZE) // 16384 frames
#define USER_FRAMES (USER_MEMORY / PAGE_SIZE) // 12288 frames
#define PAGE_TABLE_SIZE 2048 // Entries per process
#define ESSENTIAL_PAGES 10 // Essential pages per process
#define NFFMIN 1000 // Minimum number of frames for NFF

// int NFFMIN = 1000; // Minimum number of frames for NFF

// Process structure
typedef struct process {
    uint16_t page_table[PAGE_TABLE_SIZE]; // Page table (MSB is valid/invalid bit)
    int size;            // Size of array A
    int *search_keys;    // Array of search keys
    int num_searches;    // Total number of searches
    int current_search;  // ind of current search
    int swapped_out;     // Flag to indicate if process is swapped out
} Process;

// Free frames structure 
typedef struct free_frame {
    int frame_number;   // Frame number
    int last_owner;     // PID of last owner
    int page_number;   // Page number of last owner
} FreeFrame;

// Global variables
Process *processes;   // Array of processes
int n, m;             // Number of processes and searches per process
int num_freeframes; // Number of free frames
FreeFrame *FFLIST;   // List of free frames
int ind;
int *free_frames; // List of available frames
int front, rear;    // Queue front and rear indices
int count; // Number of available frames
int *ready_queue;     // Queue of processes ready to execute
int ready_front, ready_rear;  // Queue front and rear indices
int ready_count;      // Number of processes in ready queue
uint16_t *counter;

int active_processes; // Number of active (not swapped out) processes
int min_active_processes; // Minimum number of active processes when memory is full
int page_accesses;    // Total number of page accesses
int page_faults;      // Total number of page faults
int swap_operations;  // Total number of swap operations


void enqueue_free( int frame_number ){
    free_frames[rear] = frame_number;
    rear = (rear + 1) % USER_FRAMES;
    count++;
}

int dequeue_free(){
    int frame_number = free_frames[front];
    front = (front + 1) % USER_FRAMES;
    count--;
    return frame_number;
}

void enqueue_ready(int proc_id) {
    ready_queue[ready_rear] = proc_id;
    ready_rear = (ready_rear + 1) % n;
    ready_count++;
}

int dequeue_ready() {
    int proc_id = ready_queue[ready_front];
    ready_front = (ready_front + 1) % n;
    ready_count--;
    return proc_id;
}

void read_input_file(){
    FILE *fp = fopen("search.txt", "r");
    if (!fp) {
        printf("Error: Cannot open input file\n");
        exit(1);
    }

    // Read number of processes and searches per process
    fscanf(fp, "%d %d", &n, &m);

    // Allocate memory for processes
    processes = (int*)malloc(n * sizeof(int));

    // Read data for each process
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d", &processes[i]);

        processes[i].search_keys = (int*)malloc(m * sizeof(int));
        processes[i].num_searches = m;
        processes[i].current_search = 0;
        processes[i].swapped_out = 0;
        for (int j = 0; j < m; j++) {
            fscanf(fp, "%d", &processes[i].search_keys[j]);
        }

        // Initialize page table (all invalid initially)
        for( int j=0 ; j<PAGE_TABLE_SIZE ; j++ ) {
            processes[i].page_table[j] = 0;
        }
    }

    fclose(fp);
}

void initialize_kernel(){
    num_freeframes = USER_FRAMES;
    FFLIST = (FreeFrame*)malloc(USER_FRAMES * sizeof(FreeFrame));
    ind=0;
    free_frames = (int*)malloc(USER_FRAMES * sizeof(int));
    for( int i=0 ; i<USER_FRAMES ; i++ ){
        free_frames[i] = i;
    }
    count = 0;
    front = rear = 0;
    counter = (uint16_t *)malloc(USER_FRAMES * sizeof(uint16_t));
    for( int i=0 ; i<USER_FRAMES ; i++ ) {
        FFLIST[i].frame_number = i;
        FFLIST[i].last_owner = -1;
        FFLIST[i].page_number = -1;
    }

    for( int i=0 ; i<USER_FRAMES ; i++ ){
        counter[i]=0;
    }

    ready_front = ready_rear = 0;
    ready_count = 0;

    ready_queue = (int*)malloc(n * sizeof(int));

    // Initialize statistics
    active_processes = 0;
    min_active_processes = n;
    page_accesses = 0;
    page_faults = 0;
    swap_operations = 0;

    // Allocate essential pages for each process
    for ( int i=0 ; i<n ; i++ ){
        for( int j=0 ; j< ESSENTIAL_PAGES ; j++ ){
            if( num_freeframes >  NFFMIN ){
                int frame_number = dequeue_free();
                processes[i].page_table[j] = (1<<15) | frame_number;
                counter[frame_number] = 0xFFFF; // Set counter to max value
            }else{
                perror("Error: Not enough memory for initialization\n");
                exit(1);
            }
        }

        enqueue_ready(i);
    }
    
}

int handle_page_fault( int proc_id , int page_number){
    if( num_freeframes > NFFMIN ){
        // Allocate a free frame
        int frame_number = dequeue_free();
        processes[proc_id].page_table[page_number] = (1<<15) | frame_number; // Set valid bit and frame number
        processes[proc_id] = processes[proc_id] | (1<<14 );  // set reserved bit
        return 0; // Success
    }else{
        // We need to select which frame to swap out
        // rules for swapping in the order of importance is 
        // 1. select frame with same process and same page number
        // 2. select frame with no previous owner
        // 3. select frame with same process and different page number
        // 4. select frame in random order

        int frame_number = -1;
        int page_number = -1;
        int min_counter = 0xFFFF; // Initialize to max value
        int min_ind = -1;
        for( int i=0 ; i<USER_FRAMES ; i++ ){
            if( FFLIST[i].last_owner == proc_id && FFLIST[i].page_number == page_number ){
                // Rule 1: Same process and same page number
                frame_number = FFLIST[i].frame_number;
                break;
            }else if( FFLIST[i].last_owner == -1 ){
                // Rule 2: No previous owner
                frame_number = FFLIST[i].frame_number;
                break;
            }else if( FFLIST[i].last_owner == proc_id && FFLIST[i].page_number != page_number ){
                // Rule 3: Same process and different page number
                if( counter[FFLIST[i].frame_number] < min_counter ){
                    min_counter = counter[FFLIST[i].frame_number];
                    min_ind = i;
                }
            }else{
                // Rule 4: Random order
                if( counter[FFLIST[i].frame_number] < min_counter ){
                    min_counter = counter[FFLIST[i].frame_number];
                    min_ind = i;
                }
            }
        }
        if( frame_number != -1 ){
            // Swap out the frame
            processes[proc_id].page_table[page_number] = 0; // Mark as invalid
            processes[proc_id].page_table[page_number] = (1<<15) | frame_number; // Set valid bit and frame number
            processes[proc_id].page_table[page_number] = processes[proc_id].page_table[page_number] | (1<<14); // set reserved bit
            processes[proc_id].page_table[page_number] = processes[proc_id].page_table[page_number] & ~(1<<15); // clear valid bit
            FFLIST[min_ind].frame_number = frame_number;
            FFLIST[min_ind].last_owner = proc_id; // Set last owner to current process
            FFLIST[min_ind].page_number = page_number; // Set page number to current page
            return 0; // Success
        }else{
            // No free frames available, need to swap out a process
            swap_out_process(proc_id);
            return -1; // Process swapped out
        }
        return 0;
    }
}

int binary_search( int proc_id ){
    Process *proc = &processes[proc_id];
    int search_value = proc->search_keys[proc->current_search];
    int left = 0;
    int right = proc->size - 1;

    while( left < right ){
        int mid = (left +right)/2;

        page_accesses++;

        int page_num = ESSENTIAL_PAGES + (mid * sizeof(int)) / PAGE_SIZE;

        if(!(proc->page_table[page_num] & (1<<15))){
            // Page fault
            page_faults++;
            int result = handle_page_fault(proc_id, page_num);
            if( result == -1 ){
                return -1;
            }
            // set the reserved bit 
            processes[proc_id].page_table[page_num] = processes[proc_id].page_table[page_num] | (1<<14); // set reserved bit
        } else{
            // page is valid 
            processes[proc_id].page_table[page_num] = processes[proc_id].page_table[page_num] | (1<<14); // set reserved bit
        }

        if( search_value <= mid ){
            right = mid;
        }else{
            left = mid + 1;
        }
    }

    return 0;
}

void run_simulation(){
    int completed_processes = 0;

    while(completed_processes <n ){
        if( ready_count == 0 ){
            printf("No processes in ready or swap queue. Exiting...\n");
            break;
        }
        
        if( ready_count > 0 ){
            int proc_id = dequeue_ready();

            if( processes[proc_id].swapped_out ){
                continue; // Skip if process is swapped out
            }

            if( processes[proc_id].current_search >= m ){
                // Process has finished all searches
                completed_processes++;
                
                // Free all frames allocated to this process
                for( int i=0 ; i< PAGE_TABLE_SIZE ; i++ ){
                    if( processes[proc_id].page_table[i] & (1<<15) ){
                        int frame_number = processes[proc_id].page_table[i] & 0x7FFF;
                        FFLIST[frame_number].last_owner = -1;
                        FFLIST[frame_number].page_number = -1;
                        enqueue_free(frame_number);
                    }
                }
            }else{
                // Process is still searching
                int result = binary_search(proc_id);
                if( result == 0 ){
                    processes[proc_id].current_search++;
                    enqueue_ready(proc_id);
                }
                
            }
        }
        // reset counters for all processes
        for( int i=0 ; i<n ; i++ ){
            for( int j=0 ; j<PAGE_TABLE_SIZE ; j++ ){
                if( processes[i].page_table[j] && (i<<14) ){
                    int frame_number = processes[i].page_table[j] & 0x7FFF;
                    int value = counter[frame_number];
                    value = (1<<15) | (value>>1);
                    counter[frame_number] = value;
                    processes[i].page_table[j] = processes[i].page_table[j] & ~(1<<14); // reset reserved bit
                }
            }
        }
    }
}

void print_statistics(){
    // Print statistics
}

int main() {
    read_input_file();
    initialize_kernel();
    run_simulation();
    print_statistics();

    // Clean up
    for( int i=0 ; i<n ; i++ ) {
        free(processes[i].search_keys);
    }
    free(processes);
    free(ready_queue);

    return 0;
}