//copyright

#ifndef _DOUBLE_INSTANCE_LOCKING_ARRAYLIST_H_
#define _DOUBLE_INSTANCE_LOCKING_ARRAYLIST_H_

#include <pthread.h>

typedef struct {
    int arraySize;
    int usedSize1;
    int usedSize2;
    void ** arrayList1;
    void ** arrayList2;
    pthread_mutex_t writersMutex;
    pthread_rwlock_t rwlock1;
    pthread_rwlock_t rwlock2;
} di_arraylist_t;

void di_arraylist_init(di_arraylist_t * const self, int initialSize);
void di_arraylist_destroy(di_arraylist_t * const self);
int di_arraylist_add(di_arraylist_t * const self, void * const item);
int di_arraylist_remove(di_arraylist_t * const self, void * const item);
int di_arraylist_contains(di_arraylist_t * const self, void * const item);


#endif // _DOUBLE_INSTANCE_LOCKING_ARRAYLIST_H_
