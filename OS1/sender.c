#include "sender.h"
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

// Function to notify the receiver (sem_post)


void send(message_t message, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) { // Message Passing using POSIX Message Queue
        if (mq_send(mailbox_ptr->mqdes, (char*)&message, sizeof(message), 0) == -1) {
            perror("mq_send failed");
            exit(EXIT_FAILURE);
        }
    } else if (mailbox_ptr->flag == 2) { // Shared Memory
        memcpy(mailbox_ptr->shm_addr, &message, sizeof(message));
    } else {
        fprintf(stderr, "Invalid communication flag\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mechanism> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int mechanism = atoi(argv[1]);
    const char *input_file = argv[2];
    mailbox_t mailbox;
    mailbox.flag = mechanism;

    sem_t *sem_send =sem_open("/sem_send", O_CREAT, 0666, 1); // Semaphore handle
    sem_t *sem_receive =sem_open("/sem_receive", O_CREAT, 0666, 0);

    // Setup communication based on mechanism
    if (mechanism == 1) { // POSIX Message Queue
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = sizeof(message_t);
        attr.mq_curmsgs = 0;

        mailbox.mqdes = mq_open("/mailbox_queue", O_CREAT | O_WRONLY, 0644, &attr);
        if (mailbox.mqdes == (mqd_t) -1) {
            perror("mq_open failed");
            exit(EXIT_FAILURE);
        }

        // Open semaphore for message queue
       
    } else if (mechanism == 2) { // Shared Memory
        int shm_fd = shm_open("/mailbox_shm", O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(EXIT_FAILURE);
        }
        ftruncate(shm_fd, sizeof(message_t));
        mailbox.shm_addr = mmap(NULL, sizeof(message_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }

        // Open semaphore for shared memory
    } else {
        fprintf(stderr, "Invalid mechanism. Use 1 for Message Passing or 2 for Shared Memory.\n");
        exit(EXIT_FAILURE);
    }

    // Read messages from input file
    FILE *file = fopen(input_file, "r");
    if (file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    message_t message;
    clock_t start_time;
    clock_t end_time;

    while (fgets(message.msg_text, sizeof(message.msg_text), file) != NULL) {
        // message.msg_type = 1;
        // strncpy(message.msg_text, buffer, sizeof(message.msg_text) - 1);
        // message.msg_text[sizeof(message.msg_text) - 1] = '\0';

        // Semaphore logic for both mechanisms
        // Wait for the receiver to be ready
        sem_wait(sem_send);
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        sem_post(sem_receive);
         // Notify the receiver that the message is sent
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)*1e-9;

        printf(AOI BOLD "Sending message: " RESET "%s", message.msg_text);
    }

    // Send exit message
    strcpy(message.msg_text, "\nEXIT");

    // Semaphore logic for both mechanisms
    // Wait for the receiver to be ready
    sem_wait(sem_send);
    send(message, &mailbox);
    sem_post(sem_receive);

 // Notify the receiver that the exit message is sent

    printf(RED BOLD "\nSent exit message.\n" RESET);

    
    printf("Total sending time: %lf seconds\n", time_taken);

    // Cleanup
    fclose(file);
    if (mechanism == 1) {
        mq_close(mailbox.mqdes);
        mq_unlink("/mailbox_queue");
    } else if (mechanism == 2) {
        munmap(mailbox.shm_addr, sizeof(message_t));
        shm_unlink("/mailbox_shm");
    }
    sem_close(sem_send);
    sem_close(sem_send);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");
    
    return 0;
}

