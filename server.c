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


// static int listenfd_global;
struct ThreadPool tp;
struct Queue request_queue;
static struct Server_Log* copy_log;

void clean_up_server(int sig){

      tp.shutDown = 1;
//    pthread_mutex_lock(&tp.lock);
//    pthread_cond_broadcast(&tp.queue_not_empty);
//    pthread_cond_broadcast(&tp.queue_not_full);
//    pthread_mutex_unlock(&tp.lock);


    for (int i = 0; i < tp.thread_count; i++)
        pthread_cancel(tp.threads[i]);

    ThreadPool_destroy(&tp);
    queue_destroy(&request_queue);
    destroy_log(copy_log);
    printf("Syscall :)\n");
    exit(0);
}

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
        app_error("error: invalid arguments");
    }
}

int main(int argc, char *argv[])
{

    server_log log = create_log();
    copy_log = log;

    signal(SIGINT,clean_up_server);
    signal(SIGTERM,clean_up_server);
    int listenfd, connfd, port, thread_count, queue_size, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, &thread_count, &queue_size, argc, argv);
    listenfd = Open_listenfd(port);
    //create the request queue
    queue_init(&request_queue, queue_size);


//    listenfd_global = Open_listenfd(port);

    //create the thread pool
    initThreadPool(&tp, thread_count, &request_queue, log);
//    initThreadPool(&tp, thread_count);

    while (1) {

        clientlen = sizeof(clientaddr);
        printf("Listening...\n");
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        printf("Got a Connection! fd is: %d\n", connfd);

//        struct sockaddr_in clientaddr;
//        int connfd = Accept(listenfd_global,
//                            (SA *)&clientaddr,
//                            &clientlen);
        if (connfd < 0) {
            // closing listenfd_global in the handler makes accept() fail
            break;
        }

        struct timeval arrival;
        gettimeofday(&arrival, NULL);

        //critical section - pushing to queue
        pthread_mutex_lock(&tp.lock);
        while (tp.request_queue->capacity == tp.request_queue->queue_size) {
            printf("waiting\n");
            pthread_cond_wait(&tp.queue_not_full, &tp.lock);
            printf("finished waiting\n");
        }

        queue_push(tp.request_queue, connfd, arrival);
        printf("queue capacity is: %d\n", tp.request_queue->capacity);
        printf("connfd: %d is pushed\n", connfd);
        tp.request_queue->capacity++;
        printf("queue capacity now is: %d\n", tp.request_queue->capacity);
        pthread_cond_signal(&tp.queue_not_empty);
        pthread_mutex_unlock(&tp.lock);
    }

    clean_up_server(0);
    return 0;

}




