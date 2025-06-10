
#ifndef OS3_THREADPOOL_H
#define OS3_THREADPOOL_H

#include "segel.h"
#include "queue.h"
#include "request.h"

//create a struct which is the threadpool
struct ThreadPool{
    int thread_count;
    // a lock for the queue
    pthread_mutex_t lock;
    // a condition var of some kind - if queue is not empty we want to assign a request to a thread
    pthread_cond_t request_available;
    // a pointer of the queue?
    struct Queue* request_queue;
    // a pointer of the server
    server_log log;

};

void* workerThread(void* var);

//thread_create all threads from 1 to thread_count to run with workerThread
void initThreadPool(struct ThreadPool* tp, int thread_count, struct Queue* request_queue, server_log log){
    tp->thread_count = thread_count;
    pthread_mutex_init(&tp->lock, NULL);
    pthread_cond_init(&tp->request_available, NULL);
    tp->request_queue = request_queue;
    tp->log = log;
    pthread_t* threads = malloc(sizeof(pthread_t)*thread_count);

    for(int i = 0; i < thread_count; i++){
        pthread_create(&threads[i], NULL, workerThread, (void *)tp);
    }

}


//passed function is a function which is joined for all threads
void* workerThread(void* var){
    struct ThreadPool* tp = (struct ThreadPool*) var;
    struct Queue* request_queue = tp->request_queue;

    //lock the mutex - while handling the shared queue because it is a critical section
    pthread_mutex_lock(&tp->lock);
    if(request_queue->head == NULL){
        pthread_cond_wait(&tp->request_available, &tp->lock);//cond_wait for q not to be empty, use lock1
    }
    int fd = request_queue->head->fd;
    struct timeval arrival = request_queue->head->arrival;
    //remove head from queue
    queue_pop(request_queue);
    //unlock lock 1
    pthread_mutex_unlock(&tp->lock);

    //assign request to thread - use request handler
    struct timeval dispatch;
    gettimeofday(&dispatch, NULL);
    threads_stats t = malloc(sizeof(struct Threads_stats));
    t->id = 0;             // Thread ID (placeholder)
    t->stat_req = 0;       // Static request count
    t->dynm_req = 0;       // Dynamic request count
    t->total_req = 0;      // Total request count
    requestHandle(fd, arrival, dispatch,t ,tp->log); //TODO: Guy - figure out what is t_stats (the missing param)

    //lock lock1
    pthread_mutex_lock(&tp->lock);
    //after thread finished update queue capacity down by 1
    request_queue->capacity--;
    //make sure that finished thread will try to run again, maybe call worker thread again
    //or maybe call worker thread in an endless loop
    //unlock lock 1
    pthread_mutex_unlock(&tp->lock);

}


#endif //OS3_THREADPOOL_H
