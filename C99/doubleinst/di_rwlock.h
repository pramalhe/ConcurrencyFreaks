#ifndef __DOUBLE_INST_RWLOCK_H__
#define __DOUBLE_INST_RWLOCK_H__

#include <pthread.h>

typedef struct {
    pthread_key_t key;
    pthread_mutex_t writersMutex;
    pthread_rwlock_t rwlock1;
    pthread_rwlock_t rwlock2;
    void *instance1;
    void *instance2;
} di_rwlock_t;


int di_rwlock_init(di_rwlock_t * self, void *instance1, void *instance2);
int di_rwlock_destroy(di_rwlock_t * self);
void *di_rwlock_rdlock(di_rwlock_t * self);
void di_rwlock_rdunlock(di_rwlock_t * self);
void *di_rwlock_rdlock_held(di_rwlock_t * self, pthread_rwlock_t * held_lock);
void di_rwlock_rdunlock_held(di_rwlock_t * self, pthread_rwlock_t * held_lock);
void *di_rwlock_wrlock(di_rwlock_t * self);
void *di_rwlock_wrtoggle(di_rwlock_t * self);
void di_rwlock_wrunlock(di_rwlock_t * self);


#endif /* __DOUBLE_INST_RWLOCK_H__ */
