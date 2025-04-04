#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define PAGE_SIZE 4096
#define TOTAL_MEMORY (64 * 1024 * 1024)
#define OS_MEMORY (16 * 1024 * 1024)
#define USER_MEMORY (TOTAL_MEMORY - OS_MEMORY)
#define TOTAL_FRAMES (TOTAL_MEMORY / PAGE_SIZE)
#define USER_FRAMES (USER_MEMORY / PAGE_SIZE)
#define PAGE_TABLE_SIZE 2048
#define ESSENTIAL_PAGES 10
#define NFFMIN 1000

typedef struct process {
    uint16_t page_table[PAGE_TABLE_SIZE];
    uint16_t page_history[PAGE_TABLE_SIZE];
    int size;
    int *search_keys;
    int num_searches;
    int current_search;
    int swapped_out;
    int page_accesses;
    int page_faults;
    int page_replacements;
    int attempt_stats[4];
} Process;

typedef struct free_frame {
    int frame_number;
    int last_owner;
    int page_number;
} FreeFrame;

Process *processes;
int n, m;
int num_freeframes;
FreeFrame *FFLIST;
int *free_frames;
int front, rear;
int count;
int *ready_queue;
int ready_front, ready_rear;
int ready_count;
int active_processes;
int min_active_processes;
int page_accesses;
int page_faults;
int swap_operations;
int total_attempt_stats[4];

void enqueue_free(int frame_number) {
    free_frames[rear] = frame_number;
    rear = (rear + 1) % USER_FRAMES;
    count++;
}

void add_to_free_front(int frame_number) {
    front = (front - 1 + USER_FRAMES) % USER_FRAMES;
    free_frames[front] = frame_number;
    count++;
}

int dequeue_free() {
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

void read_input_file() {
    FILE *fp = fopen("search.txt", "r");
    if (!fp) {
        printf("Error: Cannot open input file\n");
        exit(1);
    }
    fscanf(fp, "%d %d", &n, &m);
    processes = (Process*)malloc(n * sizeof(Process));
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d", &processes[i].size);
        processes[i].search_keys = (int*)malloc(m * sizeof(int));
        processes[i].num_searches = m;
        processes[i].current_search = 0;
        processes[i].swapped_out = 0;
        processes[i].page_accesses = 0;
        processes[i].page_faults = 0;
        processes[i].page_replacements = 0;
        for (int j = 0; j < 4; j++) {
            processes[i].attempt_stats[j] = 0;
        }
        for (int j = 0; j < m; j++) {
            fscanf(fp, "%d", &processes[i].search_keys[j]);
        }
        for (int j = 0; j < PAGE_TABLE_SIZE; j++) {
            processes[i].page_table[j] = 0;
            processes[i].page_history[j] = 0;
        }
    }
    fclose(fp);
}

void initialize_kernel() {
    FFLIST = (FreeFrame*)malloc(USER_FRAMES * sizeof(FreeFrame));
    for (int i = 0; i < USER_FRAMES; i++) {
        FFLIST[i].frame_number = i;
        FFLIST[i].last_owner = -1;
        FFLIST[i].page_number = -1;
    }
    free_frames = (int*)malloc(USER_FRAMES * sizeof(int));
    front = rear = 0;
    count = 0;
    for (int i = 0; i < USER_FRAMES; i++) {
        enqueue_free(i);
    }
    num_freeframes = USER_FRAMES;
    ready_queue = (int*)malloc(n * sizeof(int));
    ready_front = ready_rear = 0;
    ready_count = 0;
    page_accesses = 0;
    page_faults = 0;
    swap_operations = 0;
    for (int i = 0; i < 4; i++) {
        total_attempt_stats[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < ESSENTIAL_PAGES; j++) {
            if (num_freeframes > NFFMIN) {
                int frame_number = dequeue_free();
                processes[i].page_table[j] = (1<<15) | frame_number;
                processes[i].page_history[j] = 0xFFFF;
                num_freeframes--;
            } else {
                fprintf(stderr, "Error: Not enough memory for initialization\n");
                exit(1);
            }
        }
        enqueue_ready(i);
    }
}

void update_history(int proc_id) {
    Process *proc = &processes[proc_id];
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        if (proc->page_table[i] & (1<<15)) {
            proc->page_history[i] = (proc->page_history[i] >> 1) |
                ((proc->page_table[i] & (1<<14)) ? 0x8000 : 0);
            proc->page_table[i] &= ~(1<<14);
        }
    }
}

int find_victim_page(int proc_id) {
    Process *proc = &processes[proc_id];
    uint16_t min_history = 0xFFFF;
    int victim_page = -1;
    for (int i = ESSENTIAL_PAGES; i < PAGE_TABLE_SIZE; i++) {
        if ((proc->page_table[i] & (1<<15)) && (proc->page_history[i] < min_history)) {
            min_history = proc->page_history[i];
            victim_page = i;
        }
    }
    return victim_page;
}

int handle_page_fault(int proc_id, int page_number) {
    Process *proc = &processes[proc_id];
    if (num_freeframes > NFFMIN) {
        int frame_number = dequeue_free();
        proc->page_table[page_number] = (1<<15) | frame_number | (1<<14);
        proc->page_history[page_number] = 0xFFFF;
        #ifdef VERBOSE
        printf("    Free frame %d found\n", frame_number);
        #endif
        num_freeframes--;
        return 0;
    } else {
        int victim_page = find_victim_page(proc_id);
        if (victim_page == -1) {
            return -1;
        }
        int victim_frame = proc->page_table[victim_page] & 0x3FFF;
        proc->page_table[victim_page] &= ~(1<<15);
        #ifdef VERBOSE
        printf("    To replace Page %4d at Frame %d [history = %d]\n", victim_page, victim_frame, proc->page_history[victim_page]);
        #endif
        int new_frame = -1;
        int attempt = -1;
        for (int i = num_freeframes - 1; i >= 0; i--) {
            int idx = (front + i) % USER_FRAMES;
            int frame_idx = free_frames[idx];
            if (FFLIST[frame_idx].last_owner == proc_id && FFLIST[frame_idx].page_number == page_number) {
                new_frame = frame_idx;
                attempt = 0;
                for (int j = i; j < num_freeframes - 1; j++) {
                    int next_idx = (front + j + 1) % USER_FRAMES;
                    free_frames[(front + j) % USER_FRAMES] = free_frames[next_idx];
                }
                count--;
                rear = (rear - 1 + USER_FRAMES) % USER_FRAMES;
                #ifdef VERBOSE
                printf("     Attempt 1: Page found in free frame %d\n", frame_idx);
                #endif
                break;
            }
        }
        if (new_frame == -1) {
            for (int i = num_freeframes - 1; i >= 0; i--) {
                int idx = (front + i) % USER_FRAMES;
                int frame_idx = free_frames[idx];
                if (FFLIST[frame_idx].last_owner == -1) {
                    new_frame = frame_idx;
                    attempt = 1;
                    for (int j = i; j < num_freeframes - 1; j++) {
                        int next_idx = (front + j + 1) % USER_FRAMES;
                        free_frames[(front + j) % USER_FRAMES] = free_frames[next_idx];
                    }
                    count--;
                    rear = (rear - 1 + USER_FRAMES) % USER_FRAMES;
                    #ifdef VERBOSE
                    printf("     Attempt 2: Free frame %d owned by no process found\n", frame_idx);
                    #endif
                    break;
                }
            }
        }
        if (new_frame == -1) {
            for (int i = num_freeframes - 1; i >= 0; i--) {
                int idx = (front + i) % USER_FRAMES;
                int frame_idx = free_frames[idx];
                if (FFLIST[frame_idx].last_owner == proc_id) {
                    new_frame = frame_idx;
                    attempt = 2;
                    for (int j = i; j < num_freeframes - 1; j++) {
                        int next_idx = (front + j + 1) % USER_FRAMES;
                        free_frames[(front + j) % USER_FRAMES] = free_frames[next_idx];
                    }
                    count--;
                    rear = (rear - 1 + USER_FRAMES) % USER_FRAMES;
                    #ifdef VERBOSE
                    printf("     Attempt 3: Own page %d found in free frame %d\n", FFLIST[frame_idx].page_number, frame_idx);
                    #endif
                    break;
                }
            }
        }
        if (new_frame == -1) {
            int random_index = rand() % num_freeframes;
            int idx = (front + random_index) % USER_FRAMES;
            new_frame = free_frames[idx];
            attempt = 3;
            for (int j = random_index; j < num_freeframes - 1; j++) {
                int next_idx = (front + j + 1) % USER_FRAMES;
                free_frames[(front + j) % USER_FRAMES] = free_frames[next_idx];
            }
            count--;
            rear = (rear - 1 + USER_FRAMES) % USER_FRAMES;
            #ifdef VERBOSE
            printf("     Attempt 4: Free frame %d owned by Process %d chosen\n", new_frame, FFLIST[new_frame].last_owner);
            #endif
        }
        proc->page_replacements++;
        proc->attempt_stats[attempt]++;
        total_attempt_stats[attempt]++;
        proc->page_table[page_number] = (1<<15) | new_frame | (1<<14);
        proc->page_history[page_number] = 0xFFFF;
        FFLIST[victim_frame].last_owner = proc_id;
        FFLIST[victim_frame].page_number = victim_page;
        if (attempt == 1) {
            add_to_free_front(victim_frame);
        } else {
            enqueue_free(victim_frame);
        }
        return 0;
    }
}

int binary_search(int proc_id) {
    Process *proc = &processes[proc_id];
    int search_value = proc->search_keys[proc->current_search];
    #ifdef VERBOSE
    printf("+++ Process %d: Search %d\n", proc_id, proc->current_search + 1);
    #endif
    int left = 0;
    int right = proc->size - 1;
    while (left < right) {
        int mid = (left + right) / 2;
        int page_num = ESSENTIAL_PAGES + (mid * sizeof(int)) / PAGE_SIZE;
        proc->page_accesses++;
        page_accesses++;
        if (!(proc->page_table[page_num] & (1<<15))) {
            proc->page_faults++;
            page_faults++;
            #ifdef VERBOSE
            printf("    Fault on Page %4d: ", page_num);
            #endif
            int result = handle_page_fault(proc_id, page_num);
            if (result == -1) {
                return -1;
            }
        } else {
            proc->page_table[page_num] |= (1<<14);
        }
        if (search_value <= mid) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    update_history(proc_id);
    return 0;
}

void run_simulation() {
    int completed_processes = 0;
    while (completed_processes < n) {
        if (ready_count > 0) {
            int proc_id = dequeue_ready();
            if (processes[proc_id].current_search >= m) {
                completed_processes++;
                for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
                    if (processes[proc_id].page_table[i] & (1<<15)) {
                        int frame_number = processes[proc_id].page_table[i] & 0x3FFF;
                        FFLIST[frame_number].last_owner = -1;
                        FFLIST[frame_number].page_number = -1;
                        enqueue_free(frame_number);
                        num_freeframes++;
                    }
                }
            } else {
                int result = binary_search(proc_id);
                if (result == 0) {
                    processes[proc_id].current_search++;
                    enqueue_ready(proc_id);
                }
            }
        }
    }
}

void print_statistics() {
    printf("+++ Page access summary\n");
    printf(" PID Accesses Faults         Replacements                        Attempts\n");
    int total_accesses = 0;
    int total_faults = 0;
    int total_replacements = 0;
    for (int i = 0; i < n; i++) {
        Process *proc = &processes[i];
        float fault_percent = (proc->page_faults * 100.0) / (proc->page_accesses > 0 ? proc->page_accesses : 1);
        float replace_percent = (proc->page_replacements * 100.0) / (proc->page_accesses > 0 ? proc->page_accesses : 1);
        float attempt_percent[4] = {0};
        for (int j = 0; j < 4; j++) {
            if (proc->page_replacements > 0) {
                attempt_percent[j] = (proc->attempt_stats[j] * 100.0) / proc->page_replacements;
            }
        }
        printf(" %-3d %-9d %-6d(%.2f%%) %-5d(%.2f%%) %6d + %3d + %3d + %3d (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
               i, proc->page_accesses, proc->page_faults, fault_percent,
               proc->page_replacements, replace_percent,
               proc->attempt_stats[0], proc->attempt_stats[1], proc->attempt_stats[2], proc->attempt_stats[3],
               attempt_percent[0], attempt_percent[1], attempt_percent[2], attempt_percent[3]);
        total_accesses += proc->page_accesses;
        total_faults += proc->page_faults;
        total_replacements += proc->page_replacements;
    }
    float total_fault_percent = (total_faults * 100.0) / (total_accesses > 0 ? total_accesses : 1);
    float total_replace_percent = (total_replacements * 100.0) / (total_accesses > 0 ? total_accesses : 1);
    float total_attempt_percent[4] = {0};
    for (int i = 0; i < 4; i++) {
        if (total_replacements > 0) {
            total_attempt_percent[i] = (total_attempt_stats[i] * 100.0) / total_replacements;
        }
    }
    printf("\n Total %-9d %-6d(%.2f%%) %-5d(%.2f%%) %6d + %4d + %5d + %d (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
           total_accesses, total_faults, total_fault_percent,
           total_replacements, total_replace_percent,
           total_attempt_stats[0], total_attempt_stats[1], total_attempt_stats[2], total_attempt_stats[3],
           total_attempt_percent[0], total_attempt_percent[1], total_attempt_percent[2], total_attempt_percent[3]);
}


int main() {
    srand(time(NULL));
    read_input_file();
    initialize_kernel();
    run_simulation();
    print_statistics();
    
    for (int i = 0; i < n; i++) {
        free(processes[i].search_keys);
    }
    
    free(processes);
    free(FFLIST);
    free(free_frames);
    free(ready_queue);
    
    return 0;
}
