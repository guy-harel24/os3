#include "segel.h"
#include "request.h"
#include "log.h"
#include "queue.h"
#include "threadPool.h"

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Parses command-line arguments
void getargs(int *port, int* thread_count, int* queue_size, int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size> \n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *thread_count = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if(*port <= 0 || *thread_count <= 0 || *queue_size <= 0){
    }
}

int main(int argc, char *argv[])
{

    // Create the global server log
    server_log log = create_log();

    int listenfd, connfd, port, thread_count, queue_size, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, &thread_count, &queue_size, argc, argv);
    listenfd = Open_listenfd(port);
    //create the request queue
    struct Queue request_queue;
    queue_init(&request_queue, queue_size);
    //create the thread pool
    struct ThreadPool tp;
    initThreadPool(&tp, thread_count, &request_queue, log);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        struct timeval arrival;
        gettimeofday(&arrival, NULL);
        // TODO: Guy - work with request queue here

        //critical section - pushing to queue
        pthread_mutex_lock(&tp.lock);
        if(tp.request_queue->capacity == tp.request_queue->queue_size) {
            pthread_cond_wait(&tp.queue_not_full, &tp.lock);
        }
        queue_push(tp.request_queue, connfd, arrival);
        tp.request_queue->capacity++;
        pthread_cond_signal(&tp.queue_not_empty);
        pthread_mutex_unlock(&tp.lock);
        Close(connfd); // Close the connection
    }

    // Clean up the server log before exiting
    destroy_log(log);

    // TODO: HW3 — Add cleanup code for thread pool and queue
}
