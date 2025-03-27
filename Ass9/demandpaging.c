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

// Process structure
typedef struct {
    uint16_t page_table[PAGE_TABLE_SIZE]; // Page table (MSB is valid/invalid bit)
    int size;            // Size of array A
    int *search_keys;    // Array of search keys
    int num_searches;    // Total number of searches
    int current_search;  // Index of current search
    int swapped_out;     // Flag to indicate if process is swapped out
} Process;

// Global variables
Process *processes;      // Array of all processes
int n, m;                // Number of processes and searches per process
int *free_frames;        // List of available frames
int free_frames_count;   // Number of available frames
int *ready_queue;        // Queue of processes ready to execute
int ready_front, ready_rear;  // Queue front and rear indices
int ready_count;         // Number of processes in ready queue
int *swap_queue;         // Queue of swapped-out processes
int swap_front, swap_rear;   // Queue front and rear indices
int swap_count;          // Number of processes in swap queue
int active_processes;    // Number of active (not swapped out) processes
int min_active_processes; // Minimum number of active processes when memory is full
int page_accesses;       // Total number of page accesses
int page_faults;         // Total number of page faults
int swap_operations;     // Total number of swap operations


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

void enqueue_swap(int proc_id) {
    swap_queue[swap_rear] = proc_id;
    swap_rear = (swap_rear + 1) % n;
    swap_count++;
}

int dequeue_swap() {
    int proc_id = swap_queue[swap_front];
    swap_front = (swap_front + 1) % n;
    swap_count--;
    return proc_id;
}

void swap_in_process(int proc_id) {
    processes[proc_id].swapped_out = 0;
    
    // Allocate essential pages
    for (int i = 0; i < ESSENTIAL_PAGES; i++) {
        int frame = free_frames[--free_frames_count];
        processes[proc_id].page_table[i] = (1 << 15) | frame;
    }
    
    // Only increment active_processes if it's below 40
    if (active_processes < 40) {
        active_processes++;
    }
    
    printf("+++ Swapping in process %4d  [%d active processes]\n", proc_id, active_processes);
}


// Swap out a process
void swap_out_process(int proc_id) {
    swap_operations++;
    processes[proc_id].swapped_out = 1;
    
    // Free essential pages
    for (int i = 0; i < ESSENTIAL_PAGES; i++) {
        if (processes[proc_id].page_table[i] & (1 << 15)) {
            int frame = processes[proc_id].page_table[i] & 0x7FFF;
            free_frames[free_frames_count++] = frame;
            processes[proc_id].page_table[i] = 0; // Invalid
        }
    }
    
    // Free array pages
    for (int i = ESSENTIAL_PAGES; i < PAGE_TABLE_SIZE; i++) {
        if (processes[proc_id].page_table[i] & (1 << 15)) {
            int frame = processes[proc_id].page_table[i] & 0x7FFF;
            free_frames[free_frames_count++] = frame;
            processes[proc_id].page_table[i] = 0; // Invalid
        }
    }
    
    active_processes--;
    if (active_processes < min_active_processes) {
        min_active_processes = active_processes;
    }
    
    printf("+++ Swapping out process %4d  [%d active processes]\n", proc_id, active_processes);
    
    // Add to swap queue
    enqueue_swap(proc_id);
}

// Read input from file
void read_input_file() {
    FILE *fp = fopen("search.txt", "r");
    if (!fp) {
        printf("Error: Cannot open input file\n");
        exit(1);
    }
    
    // Read number of processes and searches per process
    fscanf(fp, "%d %d", &n, &m);
    
    // Allocate memory for processes
    processes = (Process *)malloc(n * sizeof(Process));
    
    // Read data for each process
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d", &processes[i].size);
        
        processes[i].search_keys = (int *)malloc(m * sizeof(int));
        processes[i].num_searches = m;
        processes[i].current_search = 0;
        processes[i].swapped_out = 0;
        
        for (int j = 0; j < m; j++) {
            fscanf(fp, "%d", &processes[i].search_keys[j]);
        }
        
        // Initialize page table (all invalid initially)
        for (int j = 0; j < PAGE_TABLE_SIZE; j++) {
            processes[i].page_table[j] = 0;
        }
    }
    
    fclose(fp);
    printf("+++ Simulation data read from file\n");
}

// Initialize kernel data structures
void initialize_kernel() {
    // Initialize free frames
    free_frames_count = USER_FRAMES;
    free_frames = (int *)malloc(USER_FRAMES * sizeof(int));
    for (int i = 0; i < USER_FRAMES; i++) {
        free_frames[i] = i;
    }
    
    // Initialize ready queue
    ready_queue = (int *)malloc(n * sizeof(int));
    ready_front = ready_rear = ready_count = 0;
    
    // Initialize swap queue
    swap_queue = (int *)malloc(n * sizeof(int));
    swap_front = swap_rear = swap_count = 0;
    
    // Initialize statistics
    active_processes = n;
    min_active_processes = n;
    page_accesses = 0;
    page_faults = 0;
    swap_operations = 0;
    
    // Allocate essential pages for each process
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < ESSENTIAL_PAGES; j++) {
            if (free_frames_count > 0) {
                int frame = free_frames[--free_frames_count];
                processes[i].page_table[j] = (1 << 15) | frame; // Set valid bit and frame number
            } else {
                printf("Error: Not enough memory for initialization\n");
                exit(1);
            }
        }
        
        // Add process to ready queue
        enqueue_ready(i);
    }
    
    printf("+++ Kernel data initialized\n");
}

// Handle page fault
int handle_page_fault(int proc_id, int page_num) {
    if (free_frames_count > 0) {
        // Allocate a free frame
        int frame = free_frames[--free_frames_count];
        processes[proc_id].page_table[page_num] = (1 << 15) | frame;
        return 0; // Success
    } else {
        // No free frames, need to swap out a process
        swap_out_process(proc_id);
        return -1; // Process swapped out
    }
}

// Perform binary search for the current search key
int binary_search(int proc_id) {
    Process *proc = &processes[proc_id];
    int search_value = proc->search_keys[proc->current_search];
    
    int left = 0;
    int right = proc->size - 1;
    
    while (left < right) {
        int mid = (left + right) / 2;
        
        // Access A[mid]
        page_accesses++;
        
        // Calculate page number for A[mid]
        int page_num = ESSENTIAL_PAGES + (mid * sizeof(int)) / PAGE_SIZE;
        
        // Check if page is valid
        if (!(proc->page_table[page_num] & (1 << 15))) {
            // Page fault
            page_faults++;
            
            // Try to load the page
            if (handle_page_fault(proc_id, page_num) != 0) {
                // Process got swapped out
                return -1;
            }
        }
        
        // For simulation, we pretend A[mid] = mid
        if (search_value <= mid) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    return 0; // Search completed successfully
}

// Run the simulation
void run_simulation() {
    int completed_processes = 0;
    
    while (completed_processes < n) {
        if (ready_count == 0 && swap_count == 0) {
            break; // No more processes to run
        }
        
        if (ready_count > 0) {
            int proc_id = dequeue_ready();
            
            // Skip if process is swapped out
            if (processes[proc_id].swapped_out) {
                continue;
            }
            
            // Check if process has completed all searches
            if (processes[proc_id].current_search >= m) {
                // Process has finished all searches
                completed_processes++;
                
                // Free all frames allocated to this process
                for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
                    if (processes[proc_id].page_table[i] & (1 << 15)) {
                        int frame = processes[proc_id].page_table[i] & 0x7FFF;
                        free_frames[free_frames_count++] = frame;
                        processes[proc_id].page_table[i] = 0; // Mark as invalid
                    }
                }
                
                // Swap in a waiting process if any
                if (swap_count > 0) {
                    int waiting_proc = dequeue_swap();
                    swap_in_process(waiting_proc);
                    
                    // Restart the search that caused the swap-out
                    // The current_search is not incremented yet since it was interrupted
                    int result = binary_search(waiting_proc);
                    
                    if (result == 0) {
                        // Search completed successfully now
                        processes[waiting_proc].current_search++;
                    }
                    
                    // Add to ready queue regardless of search result
                    enqueue_ready(waiting_proc);
                }
            } else {
                // Perform binary search
                #ifdef VERBOSE
                printf("\tSearch %d by Process %d\n", processes[proc_id].current_search + 1, proc_id);
                #endif
                
                int result = binary_search(proc_id);
                
                if (result == 0) {
                    // Search completed successfully
                    processes[proc_id].current_search++;
                    enqueue_ready(proc_id);
                }
                // If result is -1, process was swapped out during the search
                // It will be handled when a process terminates
            }
        }
    }
}

// Print final statistics
void print_statistics() {
    printf("+++ Page access summary\n");
    printf("\tTotal number of page accesses  =  %d\n", page_accesses);
    printf("\tTotal number of page faults    =  %d\n", page_faults);
    printf("\tTotal number of swaps          =  %d\n", swap_operations);
    printf("\tDegree of multiprogramming     =  %d\n", min_active_processes);
}

int main() {
    read_input_file();
    initialize_kernel();
    run_simulation();
    print_statistics();
    
    // Clean up
    for (int i = 0; i < n; i++) {
        free(processes[i].search_keys);
    }
    free(processes);
    free(free_frames);
    free(ready_queue);
    free(swap_queue);
    
    return 0;
}