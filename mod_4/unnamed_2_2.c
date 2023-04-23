#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define SMOKERS 3 // number of smokers
#define ITEMS 3 // number of items
#define MAX_ROUNDS 10 // maximum number of rounds

// enum for items
enum item {
    TOBACCO = 0,
    PAPER = 1,
    MATCH = 2
};

// struct for shared memory
struct shared_mem {
    sem_t agent; // semaphore for agent
    sem_t smokers[SMOKERS]; // semaphores for smokers
    int table[ITEMS]; // items on the table
    int rounds; // number of rounds completed
};

// global pointer to shared memory
struct shared_mem *mem;

// function to get the index of the smoker who has the third item
int get_smoker_index(int item1, int item2) {
    return 3 - item1 - item2;
}

// function to print the name of the item
void print_item_name(int item) {
    switch (item) {
        case TOBACCO:
            printf("tobacco");
            break;
        case PAPER:
            printf("paper");
            break;
        case MATCH:
            printf("match");
            break;
        default:
            printf("unknown");
            break;
    }
}

// function to simulate the agent process
void agent(struct shared_mem *mem) {
    srand(time(NULL)); // seed random number generator
    while (1) {
        sem_wait(&mem->agent); // wait for agent semaphore
        if (mem->rounds >= MAX_ROUNDS) { // check if maximum rounds reached
            printf("Maximum rounds reached. Terminating program.\n");
            exit(0); // exit program
        }
        int item1 = rand() % ITEMS; // pick a random item
        int item2 = (item1 + 1 + rand() % (ITEMS - 1)) % ITEMS; // pick another random item
        mem->table[item1] = 1; // put the first item on the table
        mem->table[item2] = 1; // put the second item on the table
        printf("Agent puts ");
        print_item_name(item1);
        printf(" and ");
        print_item_name(item2);
        printf(" on the table.\n");
        int smoker_index = get_smoker_index(item1, item2); // get the index of the smoker who has the third item
        sem_post(&mem->smokers[smoker_index]); // signal the smoker semaphore
    }
}

// function to simulate the smoker process
void smoker(struct shared_mem *mem, int index) {
    while (1) {
        sem_wait(&mem->smokers[index]); // wait for smoker semaphore
        printf("Smoker %d has ", index);
        print_item_name(index);
        printf(".\n");
        printf("Smoker %d takes ", index);
        for (int i = 0; i < ITEMS; i++) { // loop through the items on the table
            if (mem->table[i]) { // if the item is on the table
                print_item_name(i); // print the name of the item
                printf(" and ");
                mem->table[i] = 0; // remove the item from the table
            }
        }
        printf("from the table.\n");
        printf("Smoker %d rolls and smokes a cigarette.\n", index);
        sleep(1); // simulate smoking time
        mem->rounds++; // increment rounds completed
        sem_post(&mem->agent); // signal the agent semaphore
    }
}

// function to handle keyboard interrupt signal (Ctrl+C)
void sigint_handler(int sig) {
    printf("\nKeyboard interrupt received. Terminating program.\n");
    exit(0); // exit program
}

// function to clean up resources before exiting program
void cleanup() {
    // destroy semaphores in shared memory
    sem_destroy(&mem->agent); // destroy agent semaphore
    for (int i = 0; i < SMOKERS; i++) { // loop through smokers semaphores
        sem_destroy(&mem->smokers[i]); // destroy smoker semaphore
    }

    // deallocate shared memory using munmap
    if (munmap(mem, sizeof(struct shared_mem)) == -1) { // check for errors
        perror("munmap");
        exit(1);
    }
}

// main function
int main() {
    // register signal handler for keyboard interrupt
    signal(SIGINT, sigint_handler);

    // register cleanup function to be called at exit
    atexit(cleanup);

    // allocate shared memory using mmap
    mem = mmap(NULL, sizeof(struct shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { // check for errors
        perror("mmap");
        exit(1);
    }

    // initialize semaphores in shared memory
    if (sem_init(&mem->agent, 1, 1) == -1) { // check for errors
        perror("sem_init");
        exit(1);
    }
    for (int i = 0; i < SMOKERS; i++) { // loop through smokers semaphores
        if (sem_init(&mem->smokers[i], 1, 0) == -1) { // check for errors
            perror("sem_init");
            exit(1);
        }
    }

    // initialize items on the table to 0
    for (int i = 0; i < ITEMS; i++) {
        mem->table[i] = 0;
    }

    // initialize rounds completed to 0
    mem->rounds = 0;

    // fork agent process
    pid_t pid = fork();
    if (pid == -1) { // check for errors
        perror("fork");
        exit(1);
    }
    if (pid == 0) { // child process
        agent(mem); // call agent function
        exit(0); // exit child process
    }

    // fork smoker processes
    for (int i = 0; i < SMOKERS; i++) {
        pid = fork();
        if (pid == -1) { // check for errors
            perror("fork");
            exit(1);
        }
        if (pid == 0) { // child process
            smoker(mem, i); // call smoker function with index
            exit(0); // exit child process
        }
    }

    // wait for child processes to terminate
    for (int i = 0; i < SMOKERS + 1; i++) {
        wait(NULL);
    }

    return 0;
}