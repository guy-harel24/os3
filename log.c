#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "WRLocks.h"
#include <unistd.h>
// Opaque struct definition

struct LogNode{
    const char * data;
    int data_len;
    struct LogNode *next;
    struct LogNode *prev;
};

struct Server_Log {
    // TODO: Implement internal log storage (e.g., dynamic buffer, linked list, etc.)
    struct LogNode* head;
    struct LogNode* tail;
};

void Server_Log_Init(server_log log) {
    log->head = NULL;
    log->tail = NULL;
}

// Creates a new server log instance (stub)
server_log create_log() {
    // TODO: Allocate and initialize internal log structure
    server_log log = (server_log)malloc(sizeof(struct Server_Log));
    if(!log){
        app_error("error: Bad Allocation");
    }
    Server_Log_Init(log);
    readers_writers_init();
    return log;
}

// Destroys and frees the log
void destroy_log(server_log log) {
    if (!log) return;

    struct LogNode *current = log->head;
    while (current) {
        struct LogNode* temp = current;
        current = current->next;
        if (temp->data) {
            free((void*)temp->data);
        }
        free(temp);
    }
    free(log);
}

//returns the content of the log
int get_log(server_log log, char** dst) {

    reader_lock();
    int len = 1;
    struct LogNode *current = log->head;

    //count log size
    while (current){
        len += current->data_len + 1;
        current = current->next;
    }

    char *result = malloc(len);
    if (!result) {
        reader_unlock();
        app_error("error: Bad Allocation");
    }

    //copy the log content into the result string as efficiently as possible
    char *write_pos = result;
    current = log->head;
    while (current) {
        memcpy(write_pos, current->data, current->data_len);
        write_pos += current->data_len;

        *write_pos = '\n';  // Add newline
        write_pos++;

        current = current->next;
    }

    *write_pos = '\0';
    *dst = result;
    reader_unlock();
    //return number of chars in the log - not including NULL termination
    return len - 1;

}

// Appends a new entry to the log
void add_to_log(server_log log, const char* data, int data_len) {

    // This function should handle concurrent access
    // writer_lock(); - this lock was moved down since this is not a critical section
    // creating logEntry
    struct LogNode *newLog = malloc(sizeof(struct LogNode));
    if (!newLog){
        app_error("error: Bad Allocation");
    }
    char *data_copy = malloc(data_len + 1);
    if (!data_copy){
        app_error("error: Bad Allocation");
    }

    memcpy(data_copy, data, data_len);
    data_copy[data_len] = '\0';
    newLog->data = data_copy;
    newLog->data_len = data_len;
    newLog->next = NULL;

    // critical section of changing the shared log
    // pushing entry to end of the log
    writer_lock();
    if(log->tail) {
        log->tail->next = newLog;
        newLog->prev = log->tail;
    } else {
        log->head = newLog;
        newLog->prev = NULL;
    }
    usleep(200000); // set this to 0.2 seconds (in microseconds)
    log->tail = newLog;
    writer_unlock();
}