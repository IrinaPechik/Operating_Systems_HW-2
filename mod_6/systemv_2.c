#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
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
    int table[ITEMS]; // items on the table
    int rounds; // number of rounds completed
};

// global pointer to shared memory
struct shared_mem *mem;

// global ids for semaphores and shared memory
int semid; // semaphore id
int shmid; // shared memory id

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
        struct sembuf op; // semaphore operation struct
        op.sem_flg = 0; // set flags to 0
        op.sem_num = SMOKERS; // set semaphore number to agent semaphore
        op.sem_op = -1; // set operation to decrement by 1
        semop(semid, &op, 1); // perform semaphore operation and wait for agent semaphore
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
        op.sem_num = smoker_index; // set semaphore number to smoker semaphore
        op.sem_op = 1; // set operation to increment by 1
        semop(semid, &op, 1); // perform semaphore operation and signal smoker semaphore
    }
}
// function to simulate the smoker process
void smoker(struct shared_mem *mem, int index) {
    while (1) {
        struct sembuf op; // semaphore operation struct
        op.sem_flg = 0; // set flags to 0
        op.sem_num = index; // set semaphore number to smoker semaphore
        op.sem_op = -1; // set operation to decrement by 1
        semop(semid, &op, 1); // perform semaphore operation and wait for smoker semaphore
        printf("Smoker %d has ", index);
        print_item_name(index);
        printf(".\n");
        printf("Smoker %d takes ", index);
        for (int i = 0; i < ITEMS; i++) { // loop through items on the table
            if (mem->table[i] == 1) { // check if item is on the table
                print_item_name(i);
                printf(" and ");
                mem->table[i] = 0; // remove item from the table
            }
        }
        printf("from the table.\n");
        printf("Smoker %d rolls and smokes a cigarette.\n", index);
        sleep(1); // simulate smoking time
        mem->rounds++; // increment rounds completed
        op.sem_num = SMOKERS; // set semaphore number to agent semaphore
        op.sem_op = 1; // set operation to increment by 1
        semop(semid, &op, 1); // perform semaphore operation and signal agent semaphore
    }
}

// function to handle keyboard interrupt signal
void sigint_handler(int sig) {
    printf("Keyboard interrupt received. Terminating program.\n");
    exit(0); // exit program
}

// function to clean up semaphores and shared memory at exit
void cleanup() {
    // remove semaphores using semctl
    if (semctl(semid, 0, IPC_RMID) == -1) { // check for errors
        perror("semctl");
    }

    // detach shared memory using shmdt
    if (shmdt(mem) == -1) { // check for errors
        perror("shmdt");
    }

    // remove shared memory using shmctl
    if (shmctl(shmid, IPC_RMID, NULL) == -1) { // check for errors
        perror("shmctl");
    }
}

// main function
int main() {
    // register signal handler for keyboard interrupt
    signal(SIGINT, sigint_handler);

    // register cleanup function to be called at exit
    atexit(cleanup);

    // create a key for semaphores and shared memory using ftok
    key_t key = ftok(".", 's'); // use current directory and 's' as key parameters
    if (key == -1) { // check for errors
        perror("ftok");
        exit(1);
    }

    // create semaphores using semget and IPC_CREAT flag
    semid = semget(key, SMOKERS + 1, IPC_CREAT | 0666); // create SMOKERS + 1 semaphores with read-write permissions
    if (semid == -1) { // check for errors
        perror("semget");
        exit(1);
    }

    // initialize semaphores using semctl and SETVAL command
    if (semctl(semid, SMOKERS, SETVAL, 1) == -1) { // initialize agent semaphore to 1 and check for errors
        perror("semctl");
        exit(1);
    }
    for (int i = 0; i < SMOKERS; i++) { // loop through smoker semaphores
        if (semctl(semid, i, SETVAL, 0) == -1) { // initialize smoker semaphore to 0 and check for errors
            perror("semctl");
            exit(1);
        }
    }

    // create shared memory using shmget and IPC_CREAT flag
    shmid = shmget(key, sizeof(struct shared_mem), IPC_CREAT | 0666); // create shared memory segment with read-write permissions
    if (shmid == -1) { // check for errors
        perror("shmget");
        exit(1);
    }

    // attach shared memory using shmat
    mem = (struct shared_mem *) shmat(shmid, NULL, 0); // attach shared memory segment to global pointer and check for errors
    if (mem == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // initialize shared memory
    for (int i = 0; i < ITEMS; i++) { // loop through items on the table
        mem->table[i] = 0; // set item to 0 (not on the table)
    }
    mem->rounds = 0; // set rounds completed to 0

    // fork child processes for smokers and agent
    for (int i = 0; i < SMOKERS + 1; i++) { // loop through SMOKERS + 1 processes
        pid_t pid = fork(); // fork a child process and get its pid
        if (pid == -1) { // check for errors
            perror("fork");
            exit(1);
        }
        if (pid == 0) { // check if child process
            if (i == SMOKERS) { // check if agent process
                agent(mem); // call agent function
            } else { // smoker process
                smoker(mem, i); // call smoker function with index i
            }
            exit(0); // exit child process
        }
    }

    // wait for child processes to terminate using waitpid
    for (int i = 0; i < SMOKERS + 1; i++) { // loop through SMOKERS + 1 processes
        pid_t pid = waitpid(-1, NULL, 0); // wait for any child process to terminate and get its pid
        if (pid == -1) { // check for errors
            perror("waitpid");
            exit(1);
        }
        printf("Process %d terminated.\n", pid); // print terminated process pid
    }

    return 0; // return from main function
}