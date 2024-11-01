#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>
#include <mqueue.h>

typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    mqd_t mqdes; // POSIX message queue descriptor
    char* shm_addr; // Shared memory address
} mailbox_t;

typedef struct {
    long msg_type; // 必須有的，用於消息隊列的識別
    char msg_text[1024]; // 訊息內容，大小為 1 到 1024 bytes
} message_t;

void send(message_t message, mailbox_t* mailbox_ptr);