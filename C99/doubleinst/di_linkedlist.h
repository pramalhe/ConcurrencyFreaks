//copyright

#ifndef _DOUBLE_INSTANCE_LOCKING_LINKEDLIST_H_
#define _DOUBLE_INSTANCE_LOCKING_LINKEDLIST_H_

#include <pthread.h>


typedef struct dill_node_t {
    struct dill_node_t * next1;
    struct dill_node_t * next2;
    void * item;
} dill_node_t;

typedef struct {
    dill_node_t * head1;
    dill_node_t * tail1;
    dill_node_t * head2;
    dill_node_t * tail2;
    pthread_mutex_t writersMutex;
    pthread_rwlock_t rwlock1;
    pthread_rwlock_t rwlock2;
} di_linkedlist_t;

void di_linkedlist_init(di_linkedlist_t * const self);
void di_linkedlist_destroy(di_linkedlist_t * const self);
int di_linkedlist_add(di_linkedlist_t * const self, void * const item);
int di_linkedlist_remove(di_linkedlist_t * const self, void * const item);
int di_linkedlist_contains(di_linkedlist_t * const self, void * const item);


#endif // _DOUBLE_INSTANCE_LOCKING_LINKEDLIST_H_
