//copyright

#ifndef _READER_WRITER_LOCK_LINKEDLIST_H_
#define _READER_WRITER_LOCK_LINKEDLIST_H_

#include <pthread.h>


typedef struct rwll_node_t {
    struct rwll_node_t * next;
    void * item;
} rwll_node_t;

typedef struct {
    rwll_node_t * head;
    rwll_node_t * tail;
    pthread_rwlock_t rwlock;
} rw_linkedlist_t;

void rw_linkedlist_init(rw_linkedlist_t * self);
void rw_linkedlist_destroy(rw_linkedlist_t * self);
int rw_linkedlist_add(rw_linkedlist_t * self, void * item);
int rw_linkedlist_remove(rw_linkedlist_t * self, void * item);
int rw_linkedlist_contains(rw_linkedlist_t * self, void * item);


#endif // _READER_WRITER_LOCK_LINKEDLIST_H_
