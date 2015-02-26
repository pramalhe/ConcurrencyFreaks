//copyright

#ifndef _READER_WRITER_LOCK_ARRAYLIST_H_
#define _READER_WRITER_LOCK_ARRAYLIST_H_

#include <pthread.h>

typedef struct {
    int arraySize;
    int usedSize;
    void ** arrayList;
    pthread_rwlock_t rwlock;
} rw_arraylist_t;


void rw_arraylist_init(rw_arraylist_t * self, int initialSize);
void rw_arraylist_destroy(rw_arraylist_t * self);
int rw_arraylist_add(rw_arraylist_t * self, void * item);
int rw_arraylist_remove(rw_arraylist_t * self, void * item);
int rw_arraylist_contains(rw_arraylist_t * self, void * item);

#endif // _READER_WRITER_LOCK_ARRAYLIST_H_
