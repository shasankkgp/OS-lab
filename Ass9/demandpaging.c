#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_PROCESSES 500
#define MAX_SEARCHES 100
#define TOTAL_FRAMES 12288
#define ESSENTIAL_PAGES 10
#define TOTAL_PAGES 2048

// Page table entry structure 
typedef struct {
    uint16_t entry;  // 16-bit entry with MSB as valid/invalid bit
} PageTableEntry;

// Process structure
typedef struct {
    int size;               // Size of array A
    int* search_indices;    // Search indices 
    int m;                  // Number of searches
    int current_search;     // Current search index
    PageTableEntry* page_table;  // Page table for this process
    int* frame_list;        // Frames allocated to this process
    int num_frames;         // Number of frames allocated
} Process;

// Global variables
Process* processes;
int n, m;  // Number of processes and searches per process
int num_active_processes;
int* free_frames;
int num_free_frames;
Process** swapped_out_queue;
int swapped_out_front, swapped_out_rear;

// Kernel statistics
int total_page_accesses = 0;
int total_page_faults = 0;
int total_swaps = 0;
int min_multiprogramming = TOTAL_FRAMES / ESSENTIAL_PAGES;

// Bit manipulation macros for page table
#define IS_PAGE_VALID(entry) ((entry.entry & 0x8000) >> 15)
#define SET_PAGE_VALID(entry) (entry.entry |= 0x8000)
#define CLEAR_PAGE_VALID(entry) (entry.entry &= 0x7FFF)
#define GET_PAGE_FRAME(entry) (entry.entry & 0x7FFF)
#define SET_PAGE_FRAME(entry, frame) \
    do { \
        entry.entry = (entry.entry & 0x8000) | (frame & 0x7FFF); \
    } while(0)

// Function prototypes
void read_input_file();
void initialize_kernel_data();
void swap_out_process();
void swap_in_process();
void simulate_binary_search();
void cleanup();

int main() {
    // Read input file
    read_input_file();

    // Initialize kernel data
    initialize_kernel_data();

    #ifdef VERBOSE
    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");
    #endif

    // Simulation loop
    while (num_active_processes > 0) {
        simulate_binary_search();
    }

    // Print performance summary
    printf("+++ Page access summary\n");
    printf("Total number of page accesses = %d\n", total_page_accesses);
    printf("Total number of page faults = %d\n", total_page_faults);
    printf("Total number of swaps = %d\n", total_swaps);
    printf("Degree of multiprogramming = %d\n", min_multiprogramming);

    // Cleanup
    cleanup();
    return 0;
}

void read_input_file() {
    FILE* file = fopen("search.txt", "r");
    if (!file) {
        perror("Error opening search.txt");
        exit(1);
    }

    // Read number of processes and searches
    fscanf(file, "%d %d", &n, &m);

    // Allocate memory for processes
    processes = malloc(n * sizeof(Process));
    if (!processes) {
        perror("Memory allocation failed for processes");
        exit(1);
    }

    // Read process data
    for (int i = 0; i < n; i++) {
        Process* p = &processes[i];
        p->search_indices = malloc(m * sizeof(int));
        p->page_table = malloc(TOTAL_PAGES * sizeof(PageTableEntry));
        p->frame_list = malloc((ESSENTIAL_PAGES + (TOTAL_FRAMES / m)) * sizeof(int));
        
        // Read size of array A and search indices
        fscanf(file, "%d", &p->size);
        for (int j = 0; j < m; j++) {
            fscanf(file, "%d", &p->search_indices[j]);
        }
        
        p->m = m;
        p->current_search = 0;
        p->num_frames = 0;
        
        // Initialize page table entries as invalid
        for (int j = 0; j < TOTAL_PAGES; j++) {
            p->page_table[j].entry = 0;
        }
    }

    fclose(file);
}

void initialize_kernel_data() {
    // Allocate free frames list
    free_frames = malloc(TOTAL_FRAMES * sizeof(int));
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        free_frames[i] = i;
    }
    num_free_frames = TOTAL_FRAMES;

    // Allocate swapped out queue 
    swapped_out_queue = malloc(n * sizeof(Process*));
    swapped_out_front = swapped_out_rear = -1;

    // Allocate initial essential frames for each process
    num_active_processes = n;
    for (int i = 0; i < n; i++) {
        Process* p = &processes[i];
        
        // Allocate 10 essential frames
        for (int j = 0; j < ESSENTIAL_PAGES; j++) {
            if (num_free_frames > 0) {
                int frame = free_frames[num_free_frames - 1];
                num_free_frames--;
                
                // Set page as valid and assign frame
                p->page_table[j].entry = 0;
                SET_PAGE_VALID(p->page_table[j]);
                SET_PAGE_FRAME(p->page_table[j], frame);
                p->frame_list[p->num_frames++] = frame;
            }
        }
    }
}

void swap_out_process() {
    if (num_active_processes <= 40) return;  // Keep 40 processes active

    // Find a process to swap out
    for (int i = 0; i < n; i++) {
        Process* p = &processes[i];
        
        // Skip already swapped out or finished processes
        if (p->current_search >= p->m) continue;

        // Return all its frames to free frames list
        for (int j = 0; j < p->num_frames; j++) {
            free_frames[num_free_frames++] = p->frame_list[j];
        }

        // Add to swapped out queue
        swapped_out_queue[++swapped_out_rear] = p;

        #ifdef VERBOSE
        printf("+++ Swapping out process %3d [%d active processes]\n", 
               i, num_active_processes - 1);
        #endif

        p->num_frames = 0;
        num_active_processes--;
        total_swaps++;

        // Tracking minimum multiprogramming
        if (num_active_processes < min_multiprogramming) {
            min_multiprogramming = num_active_processes;
        }

        break;
    }
}

void swap_in_process() {
    // Check if any processes are swapped out
    if (swapped_out_front == swapped_out_rear) return;

    // Move to next swapped out process
    Process* p = swapped_out_queue[++swapped_out_front];

    // Allocate 10 essential frames
    for (int j = 0; j < ESSENTIAL_PAGES; j++) {
        if (num_free_frames > 0) {
            int frame = free_frames[num_free_frames - 1];
            num_free_frames--;
            
            // Reset page table entry
            p->page_table[j].entry = 0;
            SET_PAGE_VALID(p->page_table[j]);
            SET_PAGE_FRAME(p->page_table[j], frame);
            p->frame_list[p->num_frames++] = frame;
        }
    }

    #ifdef VERBOSE
    printf("+++ Swapping in process %3d [%d active processes]\n", 
           p - processes, num_active_processes + 1);
    #endif

    num_active_processes++;
}

void simulate_binary_search() {
    // Round-robin scheduling
    for (int i = 0; i < n; i++) {
        Process* p = &processes[i];
        
        // Skip finished or swapped out processes
        if (p->current_search >= p->m) continue;

        // Binary search simulation
        int k = p->search_indices[p->current_search];
        int L = 0, R = p->size - 1;

        while (L < R) {
            int M = (L + R) / 2;

            // Simulate page access
            total_page_accesses++;

            // Check if page is loaded
            int page = M / (4096 / sizeof(int));
            if (!IS_PAGE_VALID(p->page_table[page])) {
                total_page_faults++;

                // No free frames, swap out a process
                while (num_free_frames == 0) {
                    swap_out_process();
                }

                // Allocate a new frame
                int frame = free_frames[num_free_frames - 1];
                num_free_frames--;

                // Update page table
                p->page_table[page].entry = 0;
                SET_PAGE_VALID(p->page_table[page]);
                SET_PAGE_FRAME(p->page_table[page], frame);
                p->frame_list[p->num_frames++] = frame;
            }

            // Pretend A[M] is accessed, use simulated condition
            if (k <= M) R = M;
            else L = M + 1;
        }

        // Move to next search for this process
        p->current_search++;

        // Process finished all searches
        if (p->current_search >= p->m) {
            // Return all frames
            for (int j = 0; j < p->num_frames; j++) {
                free_frames[num_free_frames++] = p->frame_list[j];
            }
            p->num_frames = 0;
            num_active_processes--;

            // Try to swap in a waiting process
            swap_in_process();
        }
    }
}

void cleanup() {
    // Free memory for each process
    for (int i = 0; i < n; i++) {
        free(processes[i].search_indices);
        free(processes[i].page_table);
        free(processes[i].frame_list);
    }

    // Free global allocations
    free(processes);
    free(free_frames);
    free(swapped_out_queue);
}