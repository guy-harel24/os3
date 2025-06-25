
#ifndef OS3_THREADPOOL_H
#define OS3_THREADPOOL_H

#include "segel.h"
#include "queue.h"
#include "request.h"
#include "HashTable.h"

//create a struct which is the threadpool
struct ThreadPool{
    int thread_count;
    // a lock for the queue
    pthread_mutex_t lock;
    // a condition var of some kind - if queue is not empty we want to assign a request to a thread
    pthread_cond_t queue_not_empty;
    // if queue is full we want to block master thread
    pthread_cond_t queue_not_full;
    // a pointer of the queue
    struct Queue* request_queue;
    // a pointer of the server
    server_log log;
    struct HashTable * statsTable;
    pthread_t * threads;
    int shutDown;

};

void* workerThread(void* var);

//thread_create all threads from 1 to thread_count to run with workerThread
void initThreadPool(struct ThreadPool* tp, int thread_count, struct Queue* request_queue, server_log log){
    tp->thread_count = thread_count;
    pthread_mutex_init(&tp->lock, NULL);
    pthread_cond_init(&tp->queue_not_empty, NULL);
    pthread_cond_init(&tp->queue_not_full, NULL);
    tp->request_queue = request_queue;
    tp->log = log;
    tp->threads = malloc(sizeof(pthread_t)*thread_count);
    tp->shutDown = 0;
    if(!tp->threads){
        app_error("error: Bad Allocation");
    }

    tp->statsTable = malloc(sizeof *tp->statsTable);
    if(!tp->statsTable){
        app_error("error: Bad Allocation");
    }

    ht_init(tp->statsTable,thread_count);

    for(int i = 0; i < thread_count; i++){
        pthread_create(&tp->threads[i], NULL, workerThread, (void *)tp);
        threads_stats t = malloc(sizeof(struct Threads_stats));
        if(!t){
            app_error("error: Bad Allocation");
        }
        t->id = i+1;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->total_req = 0;      // Total request count
        ht_insert(tp->statsTable,tp->threads[i],t);
    }

}




//passed function is a function which is joined for all threads - PROBABLY endless loop!
void* workerThread(void* var){
    struct ThreadPool* tp = (struct ThreadPool*) var;
    struct Queue* request_queue = tp->request_queue;

    //make sure that finished thread will not die, call worker thread in an endless loop
    while (1) {
        //ciritical section - lock the mutex - while handling the shared queue because it is a critical section
        pthread_mutex_lock(&tp->lock);
        if (!tp->shutDown && request_queue->head == NULL) {
            pthread_cond_wait(&tp->queue_not_empty, &tp->lock);//cond_wait for q not to be empty, use lock1
        }
        if (tp->shutDown) {
            pthread_mutex_unlock(&tp->lock);
            break;
        }
        int fd = request_queue->head->fd;
        struct timeval arrival = request_queue->head->arrival;
        //remove head from queue
        queue_pop(request_queue);
        //unlock lock 1
        pthread_mutex_unlock(&tp->lock);
        // end of critical section

        //assign request to thread - use request handler
        struct timeval dispatch;
        gettimeofday(&dispatch, NULL);
        dispatch.tv_sec = dispatch.tv_sec - arrival.tv_sec;
        dispatch.tv_usec = dispatch.tv_usec - arrival.tv_usec;
        if(dispatch.tv_usec < 0){
            dispatch.tv_sec -= 1;
            dispatch.tv_usec += 1000000;
        }

        threads_stats t = ht_find(tp->statsTable,pthread_self());
        requestHandle(fd, arrival, dispatch,t , tp->log);

        //critical section - lock mutex
        pthread_mutex_lock(&tp->lock);
        //after thread finished update queue capacity down by 1
        request_queue->capacity--;
        pthread_cond_signal(&tp->queue_not_full);
        //unlock lock 1
        pthread_mutex_unlock(&tp->lock);
        // end of critical section

        close(fd);
    }

    //dummy return
    return var;
}

void ThreadPool_destroy(struct ThreadPool* tp){

    ht_destroy(tp->statsTable);
    free(tp->threads);
    pthread_mutex_destroy(&tp->lock);
    pthread_cond_destroy(&tp->queue_not_empty);
    pthread_cond_destroy(&tp->queue_not_full);
//    destroy_log(tp->log);
//    free(tp);
}

#endif //OS3_THREADPOOL_H
