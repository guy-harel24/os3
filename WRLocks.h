
#ifndef OS3_WRLOCKS_H
#define OS3_WRLOCKS_H

#include "segel.h"


int readers_inside, writers_inside;
pthread_cond_t read_allowed;
pthread_cond_t write_allowed;
pthread_mutex_t global_lock;
int writers_waiting;


void readers_writers_init(){
    readers_inside = 0;
    writers_inside = 0;
    writers_waiting = 0;
    pthread_cond_init(&read_allowed,NULL);
    pthread_cond_init(&write_allowed,NULL);
    pthread_mutex_init(&global_lock,NULL);
}

void reader_lock(){

    pthread_mutex_lock(&global_lock);
    while (writers_inside>0 || writers_waiting > 0){
        pthread_cond_wait(&read_allowed,&global_lock);
    }
    readers_inside++;
    pthread_mutex_unlock(&global_lock);

}

void reader_unlock(){
    pthread_mutex_lock(&global_lock);
    readers_inside--;
    if(readers_inside == 0){
        pthread_cond_signal(&write_allowed);
    }
    pthread_mutex_unlock(&global_lock);
}

void writer_lock(){
    pthread_mutex_lock(&global_lock);
    writers_waiting++;
    while(writers_inside + readers_inside > 0){
        pthread_cond_wait(&write_allowed,&global_lock);
    }
    writers_waiting--;
    writers_inside++;
    pthread_mutex_unlock(&global_lock);
}

void writer_unlock(){
    pthread_mutex_lock(&global_lock);
    writers_inside--;
    if(writers_inside == 0){
        pthread_cond_broadcast(&read_allowed);
        pthread_cond_signal(&write_allowed);
    }
    pthread_mutex_unlock(&global_lock);
}






#endif //OS3_WRLOCKS_H
