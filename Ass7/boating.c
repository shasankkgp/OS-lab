#include <stdio.h>
#include <stdlib.h>

#include <string.h>   /* Needed for strcpy() */
#include <time.h>     /* Needed for time() to seed the RNG */
#include <unistd.h>   /* Needed for sleep() and usleep() */
#include <pthread.h>  /* Needed for all pthread library calls */

typedef struct{
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;    // initialised like { init_value , PTHREAD_MUTEX_INITIALIZER , PTHREAD_COND_INITIALIZER }

// GLOBAL VARIABLES
pthread_mutex_t bmtx;
pthread_mutex_t print_mtx;  // Mutex for print statements to avoid interleaved output
semaphore boat;
semaphore raider;
pthread_barrier_t EOS;

int n,m;
int *vtime;
int *rtime;
int *BA;
int *BC;
int *BT;
pthread_barrier_t *BB;  // Barriers for boat-visitor handshaking
int remaining_visitors = 0;  // Track remaining visitors for EOS synchronization
int program_ending = 0;     // To indicate program ending to boat threads other than the last one



void P(semaphore *s) {                      // wait function
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if (s->value < 0) {
        pthread_cond_wait(&s->cv, &s->mtx);       // leaves lock and wait for signal , not a busy wait 
    }
    pthread_mutex_unlock(&s->mtx);
}

void V(semaphore *s) {                         // signal function
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if (s->value <= 0) {            // signal only when there are threads waiting
        pthread_cond_signal(&s->cv);     // signals to only one waiting thread 
    }
    pthread_mutex_unlock(&s->mtx);
}


void *boat_thread( void *arg ){
    int boat_index = (int)(intptr_t)arg + 1;  // +1 for 1-based indexing in output
    pthread_mutex_lock(&print_mtx);
    printf("Boat       %-2d    Ready\n", boat_index);
    pthread_mutex_unlock(&print_mtx);
    /*
        Mark the boat to be available.
        Repeat forever: {
        Send a signal to the rider semaphore.
        Wait on the boat semaphore.
        Get the next rider.
        Mark the boat as not available.
        Read the ride time of the rider from the shared memory.
        Ride with the visitor (usleep).
        Mark the boat as available.
        If there are no more visitors (all of the n visitors have completed riding),
        synchronize with the main thread using the barrier EOS.
    }
    */
    // We need to use bmtx for mutual exclusion of shared memory
    pthread_mutex_lock(&bmtx);
    BA[boat_index-1] = 1;
    pthread_mutex_unlock(&bmtx);

    while(1){
        // checking if the program is ending
        pthread_mutex_lock(&bmtx);
        if(program_ending){
            pthread_mutex_unlock(&bmtx);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&bmtx);

        // Send a signal to the rider semaphore
        V(&raider);

        // Wait on the boat semaphore
        P(&boat);

        // Get the next rider
        pthread_mutex_lock(&bmtx);
        if (program_ending) {
            pthread_mutex_unlock(&bmtx);
            pthread_exit(NULL);
        }

        int rider_index = BC[boat_index-1];

        // Mark the boat as not available
        BA[boat_index-1] = 0;

        

        // Read the ride time of the rider from the shared memory
        int ride_time = BT[boat_index-1];
        pthread_mutex_unlock(&bmtx);

        if( rider_index == -1 ) {
            continue;
        }

        pthread_barrier_wait(&BB[boat_index-1]);

        pthread_mutex_lock(&print_mtx);
        printf("Boat       %-2d    Start of ride for visitor   %-2d\n", boat_index, rider_index);
        pthread_mutex_unlock(&print_mtx);

        // Ride with the visitor
        usleep(ride_time*100000);   // 1 min is scaled to 100 ms

        pthread_mutex_lock(&print_mtx);
        printf("Boat       %-2d    End of ride for visitor     %-2d (ride time = %-2d)\n", boat_index, rider_index, ride_time);
        pthread_mutex_unlock(&print_mtx);

        // Mark the boat as available
        pthread_mutex_lock(&bmtx);
        BA[boat_index-1] = 1;
        remaining_visitors--;
        int last_visitor = (remaining_visitors == 0);
        pthread_mutex_unlock(&bmtx);

        // If there are no more visitors
        if( last_visitor ){
            pthread_mutex_lock(&bmtx);
            program_ending = 1;   // set the flag to indicate program ending
            pthread_mutex_unlock(&bmtx);

            // Signal all boat threads , if they are waiting
            for (int i = 0; i < m; i++) {
                V(&boat);
            }
            for (int i = 0; i < n; i++) {
                V(&raider);
            }

            // synchronize with the main thread using the barrier EOS
            pthread_barrier_wait(&EOS);
            pthread_exit(NULL);
        }
    }
    return NULL;
}

void *visitor_thread( void *arg ){
    int visitor_index = (int)(intptr_t)arg + 1;  // +1 for 1-based indexing in output
    /*
        Decide a random visit time vtime and a random ride time rtime.
        Visit other attractions for vtime (usleep).
        Send a signal to the boat semaphore.
        Wait on the rider semaphore.
        Get an available boat.
        Ride for rtime (usleep).
        Leave.
    */
    // We need to use bmtx for mutual exclusion of shared memory

    // deciding a random visit time vtime and a random ride time rtime
    pthread_mutex_lock(&bmtx);
    vtime[visitor_index-1] = 30 + rand()%91 ;  // 30 to 120
    rtime[visitor_index-1] = 15 + rand()%61 ;  // 15 to 60
    pthread_mutex_unlock(&bmtx);

    pthread_mutex_lock(&print_mtx);
    printf("Visitor    %-2d    Starts sightseeing for %-3d minutes\n", visitor_index, vtime[visitor_index - 1]);
    pthread_mutex_unlock(&print_mtx);

    // visit other attractions for vtime
    usleep(vtime[visitor_index-1]*100000);    // 1 min is scaled to 100 ms

    pthread_mutex_lock(&print_mtx);
    printf("Visitor    %-2d    Ready to ride a boat (ride time = %-2d)\n", visitor_index, rtime[visitor_index - 1]);
    pthread_mutex_unlock(&print_mtx);

    // Wait on the rider semaphore
    P(&raider);

    // Get an available boat , we need to implement a busy wait here , in order to avoid starvation lave the lock and keep usleep(10000) in the loop 
    int boat_index = -1;
    while(1) {
        pthread_mutex_lock(&bmtx);
        
        for (int i = 0; i < m; i++) {
            if (BA[i] == 1) {
                boat_index = i;
                // Set boat information
                BC[boat_index] = visitor_index;
                BT[boat_index] = rtime[visitor_index-1];
                break;
            }
        }
        
        if (boat_index != -1) {
            pthread_mutex_unlock(&bmtx);
            break;
        }
        
        // If no available boat, release lock and sleep briefly to avoid starvation
        pthread_mutex_unlock(&bmtx);
        usleep(10000);  // Wait 10ms before trying again
    }

    pthread_mutex_lock(&print_mtx);
    printf("Visitor    %-2d    Finds boat %-2d\n", visitor_index, boat_index + 1);
    pthread_mutex_unlock(&print_mtx);

    // Send a signal to the boat semaphore
    V(&boat);

    // Wait for boat thread to acknowledge using barrier
    pthread_barrier_wait(&BB[boat_index]);

    // Ride for rtime
    usleep(rtime[visitor_index-1]*100000);    // 1 min is scaled to 100 ms

    pthread_mutex_lock(&print_mtx);
    printf("Visitor    %-2d    Leaving\n", visitor_index);
    pthread_mutex_unlock(&print_mtx);

    // Leave
    pthread_exit(NULL);
}

int main( int argc , char *argv[] ){
    if (argc != 3) {
        printf("Usage: %s <boats> <visitors>\n", argv[0]);
        exit(1);
    }
    
    m = atoi(argv[1]);        // boats
    n = atoi(argv[2]);        // visitors
    
    // Check input validity
    if (m < 5 || m > 10 || n < 20 || n > 100) {
        printf("Constraints: 5 ≤ m ≤ 10, 20 ≤ n ≤ 100\n");
        exit(1);
    }
    
    srand(time(NULL));

    vtime = (int *)malloc(n * sizeof(int));
    rtime = (int *)malloc(n * sizeof(int));

    // initialize the mutex and semaphores
    // Initialize the mutex and semaphores
    pthread_mutex_init(&bmtx, NULL);
    pthread_mutex_init(&print_mtx, NULL);
    
    boat = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    raider = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

    // Initialize remaining visitors count
    remaining_visitors = n;

    // global variables and arrays are used for sharing data 
    // few barriers are also used for synchronization 

    // initialize barrier EOS to 2 
    if(pthread_barrier_init(&EOS, NULL, 2)) {
        perror("Barrier initialization failed");
        exit(EXIT_FAILURE);
    }

    // BA - boat availability 
    // BC - next visiter to be boated
    // BT - boating time of a visiter 
    // BB - barrier for boat threads
    BA = (int *)malloc(m * sizeof(int));
    BC = (int *)malloc(m * sizeof(int));
    BT = (int *)malloc(m * sizeof(int));
    for(int i = 0; i < m; i++){
        BA[i] = 1;
        BC[i] = -1;
        BT[i] = 0;
    }

    // initialize BB's barier to 2
    BB = (pthread_barrier_t *)malloc(m * sizeof(pthread_barrier_t));
    for(int i = 0; i < m; i++){
        if(pthread_barrier_init(&BB[i], NULL, 2)){
            perror("Barrier initialization failed");
            exit(EXIT_FAILURE);
        }
    }

    // create m threads for boats and n threads for visitors and wait until the last rider thread completes its boating

    // create m threads for boats
    pthread_t boat_threads[m];
    for(int i = 0; i < m; i++){
        pthread_create(&boat_threads[i], NULL, boat_thread, (void *)(intptr_t)i);
    }

    // create n threads for visitors
    pthread_t visitor_threads[n];
    for( int i = 0; i < n; i++){
        pthread_create(&visitor_threads[i], NULL, visitor_thread, (void *)(intptr_t)i);
    }

    // wait until the last rider thread completes its boating
    pthread_barrier_wait(&EOS);
    // pthread_mutex_lock(&print_mtx);
    // printf("Came out of end of session barrier\n");
    // pthread_mutex_unlock(&print_mtx);

    // deletes the synchronisation and mutual_exclusion resources
    pthread_mutex_destroy(&bmtx);
    pthread_mutex_destroy(&print_mtx);
    
    pthread_mutex_destroy(&boat.mtx);
    pthread_mutex_destroy(&raider.mtx);
    pthread_cond_destroy(&boat.cv);
    pthread_cond_destroy(&raider.cv);
    pthread_barrier_destroy(&EOS);
    
    for (int i = 0; i < m; i++) {
        pthread_barrier_destroy(&BB[i]);
    }
    
    free(vtime);
    free(rtime);
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    
    return 0;
}