#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/wait.h>

#define ITEMS 3 // number of items (tobacco, paper, matches)
#define SMOKERS 3 // number of smokers

// structure to store shared memory data
struct shared_mem {
    int table[ITEMS]; // array to store items on the table
    int rounds; // variable to store rounds completed
};

// global variables for semaphores and shared memory
int semid; // semaphore id
int shmid; // shared memory id
struct shared_mem *mem; // pointer to shared memory

// function to print item name based on index
void print_item_name(int index) {
    switch (index) {
        case 0:
            printf("tobacco");
            break;
        case 1:
            printf("paper");
            break;
        case 2:
            printf("matches");
            break;
        default:
            printf("unknown");
    }
}

// function to simulate the agent process
void agent(struct shared_mem *mem) {
    while (1) {
        struct sembuf op; // semaphore operation struct
        op.sem_flg = 0; // set flags to 0
        op.sem_num = SMOKERS; // set semaphore number to agent semaphore
        op.sem_op = -1; // set operation to decrement by 1
        semop(semid, &op, 1); // perform semaphore operation and wait for agent semaphore
        printf("Agent puts ");
        int first = rand() % ITEMS; // generate first random item index
        int second = rand() % ITEMS; // generate second random item index
        while (second == first) { // ensure second item is different from first item
            second = rand() % ITEMS;
        }
        print_item_name(first);
        printf(" and ");
        print_item_name(second);
        printf(" on the table.\n");
        mem->table[first] = 1; // put first item on the table
        mem->table[second] = 1; // put second item on the table
        int smoker = ITEMS - first - second; // calculate smoker index based on items on the table
        op.sem_num = smoker; // set semaphore number to smoker semaphore
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
        for (int i = 0; i < SMOKERS; i++) { // loop through smoker semaphores
            if (semctl(semid, i, SETVAL, 0) == -1) { // initialize smoker semaphore to 0 and check for errors
                perror("semctl");
                exit(1);
            }
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