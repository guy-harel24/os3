
#ifndef OS3_1_HASHTABLE_H
#define OS3_1_HASHTABLE_H

#include "segel.h"
#include "request.h"


struct HashNode {
    pthread_t key;
    threads_stats value;
    struct HashNode * next;
    struct HashNode * prev;
};

struct LinkedList{
    struct HashNode* head;

};

void list_insert(struct LinkedList* ll,pthread_t key, threads_stats stats);
threads_stats list_find(struct LinkedList* ll,pthread_t key);


struct HashTable{
    struct LinkedList* data;
    int size;

};


void list_insert(struct LinkedList* ll,pthread_t key, threads_stats stats){
    struct HashNode * node = malloc(sizeof(struct HashNode));
    node->key = key;
    node ->value = stats;
    node->prev = NULL;
    node->next = NULL;
    if(ll->head){
        ll->head->prev = node;
        node->next = ll->head;
    }
    ll->head = node;
}

threads_stats list_find(struct LinkedList* ll,pthread_t key){
    struct HashNode * temp = ll->head;
    while (temp){
        if(temp->key == key){
            return temp->value;
        }
        temp = temp->next;
    }
    return NULL;
}

void list_destroy(struct LinkedList* ll) {
    if (!ll) return;

    struct HashNode *current = ll->head;
    while (current) {
        struct HashNode* temp = current->next;
        free(current->value);
        free(current);
        current = temp;
    }
    ll->head = NULL;
}


void ht_init(struct HashTable* ht,int ht_size) {
    ht->data = malloc(sizeof(struct LinkedList)*ht_size);
    if (!ht->data) app_error("Bad Allocation");
    ht->size = ht_size;
    for (int i = 0; i < ht_size; i++) { ht->data[i].head = NULL; }
}

void ht_insert(struct HashTable* ht,pthread_t key, threads_stats stats){
    int index = key % ht->size;
    list_insert(&ht->data[index], key, stats);
}

threads_stats ht_find(struct HashTable* ht,pthread_t key){
    int index = key % ht->size;
    return list_find(&ht->data[index], key);
}

void ht_destroy(struct HashTable* ht) {
    if (!ht) return;

    if (ht->data) {
        for (int i = 0; i < ht->size; i++) {
            list_destroy(&ht->data[i]);
        }
        free(ht->data);
    }
    free(ht);
}


#endif //OS3_1_HASHTABLE_H
