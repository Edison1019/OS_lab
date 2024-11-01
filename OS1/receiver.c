#include "receiver.h"
#include <mqueue.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#define RESET "\033[0m"
#define RED "\033[31m"
#define AOI "\033[36m"
#define BOLD "\033[1m"

struct timespec start, end;
double time_taken;

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) { // Message Passing using POSIX Message Queue
        if (mq_receive(mailbox_ptr->mqdes, (char*)message_ptr, sizeof(message_t), NULL) == -1) {
            perror("mq_receive failed");
            exit(EXIT_FAILURE);
        }
    } else if (mailbox_ptr->flag == 2) { // Shared Memory
        memcpy(message_ptr, mailbox_ptr->shm_addr, sizeof(message_t));
    } else {
        fprintf(stderr, "Invalid communication flag\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <mechanism>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int mechanism = atoi(argv[1]);
    mailbox_t mailbox;
    mailbox.flag = mechanism;
    sem_t *sem_send = sem_open("/sem_send", 0666, 0);
    sem_t *sem_receive = sem_open("/sem_receive", 0666, 0);

    // Setup communication based on mechanism
    if (mechanism == 1) { // POSIX Message Queue
        mailbox.mqdes = mq_open("/mailbox_queue", O_RDONLY);
        if (mailbox.mqdes == (mqd_t) -1) {
            perror("mq_open failed");
            exit(EXIT_FAILURE);
        }

        // Open semaphore for message queue
        

    } else if (mechanism == 2) { // Shared Memory
        int shm_fd = shm_open("/mailbox_shm", O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(EXIT_FAILURE);
        }
        // ftruncate(shm_fd, sizeof(message_t));
        mailbox.shm_addr = mmap(0, sizeof(message_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }

        // Open semaphore for shared memory
       
    } else {
        fprintf(stderr, "Invalid mechanism. Use 1 for Message Passing or 2 for Shared Memory.\n");
        exit(EXIT_FAILURE);
    }

    message_t message;
    clock_t start_time = clock();

    while (1) {
        // Semaphore logic for both mechanisms
        sem_wait(sem_receive);  // Wait for the sender to signal
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);  // Receive the message
        clock_gettime(CLOCK_MONOTONIC, &end);
        sem_post(sem_send);  // Notify the sender that the message has been received

        if (strcmp(message.msg_text, "\nEXIT") != 0)
            printf(AOI BOLD "Receiving message:" RESET "%s", message.msg_text);
        if (strcmp(message.msg_text, "\nEXIT") == 0) {
            printf(RED BOLD "\nSender EXIT!\n" RESET);
            break;
        }
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)*1e-9;
    }

    clock_t end_time = clock();
    printf("Total receiving time: %lf seconds\n", time_taken);

    // Cleanup
    if (mechanism == 1) {
        mq_close(mailbox.mqdes);
        mq_unlink("/mailbox_queue");
    } else if (mechanism == 2) {
        munmap(mailbox.shm_addr, sizeof(message_t));
        shm_unlink("/mailbox_shm");
    }

    sem_close(sem_send); // Close semaphore
    sem_close(sem_receive);


    return 0;
}
