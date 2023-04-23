#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define ITEMS 3 // number of items (tobacco, paper, matches)
#define SMOKERS 3 // number of smokers

// structure to store shared memory data
struct shared_mem {
    int table[ITEMS]; // array to store items on the table
    int rounds; // variable to store rounds completed
};

// global variables for semaphores and shared memory
sem_t *sem[SMOKERS + 1]; // array of pointers to semaphores
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
        sem_wait(sem[SMOKERS]); // wait for agent semaphore
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
        sem_post(sem[smoker]); // signal smoker semaphore
    }
}

// function to simulate the smoker process
void smoker(struct shared_mem *mem, int index) {
    while (1) {
        sem_wait(sem[index]); // wait for smoker semaphore
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
        sem_post(sem[SMOKERS]); // signal agent semaphore
    }
}

// function to handle keyboard interrupt signal
void sigint_handler(int sig) {
    printf("Keyboard interrupt received. Terminating program.\n");
    exit(0); // exit program
}

// function to clean up semaphores and shared memory at exit
void cleanup() {
    // close semaphores using sem_close
    for (int i = 0; i < SMOKERS + 1; i++) { // loop through semaphores
        if (sem_close(sem[i]) == -1) { // close semaphore and check for errors
            perror("sem_close");
        }
    }

    // unlink semaphores using sem_unlink
    for (int i = 0; i < SMOKERS + 1; i++) { // loop through semaphores
        char name[10]; // buffer to store semaphore name
        sprintf(name, "/sem%d", i); // generate semaphore name based on index
        if (sem_unlink(name) == -1) { // unlink semaphore and check for errors
            perror("sem_unlink");
        }
    }

    // unmap shared memory using munmap
    if (munmap(mem, sizeof(struct shared_mem)) == -1) { // unmap shared memory and check for errors
        perror("munmap");
    }
}

// main function
int main() {
    // register signal handler for keyboard interrupt
    signal(SIGINT, sigint_handler);

    // register cleanup function to be called at exit
    atexit(cleanup);

    // create semaphores using sem_open and O_CREAT flag
    for (int i = 0; i < SMOKERS + 1; i++) { // loop through semaphores
        char name[10]; // buffer to store semaphore name
        sprintf(name, "/sem%d", i); // generate semaphore name based on index
        sem[i] = sem_open(name, O_CREAT, 0666, 0); // create semaphore with read-write permissions and initial value 0 and check for errors
        if (sem[i] == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }

    // initialize agent semaphore using sem_post
    if (sem_post(sem[SMOKERS]) == -1) { // increment agent semaphore by 1 and check for errors
        perror("sem_post");
        exit(1);
    }
    // create shared memory using shm_open and O_CREAT flag
    int fd = shm_open("/shm", O_CREAT | O_RDWR, 0666); // create shared memory object with read-write permissions and check for errors
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // set shared memory size using ftruncate
    if (ftruncate(fd, sizeof(struct shared_mem)) == -1) { // set shared memory object size to the size of shared_mem structure and check for errors
        perror("ftruncate");
        exit(1);
    }

    // map shared memory using mmap
    mem = (struct shared_mem *) mmap(NULL, sizeof(struct shared_mem), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // map shared memory object to global pointer and check for errors
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // close shared memory file descriptor using close
    if (close(fd) == -1) { // close file descriptor and check for errors
        perror("close");
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