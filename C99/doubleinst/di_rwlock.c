/******************************************************************************
 * Copyright (c) 2013-2015, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

/**
 * <h1> Double Instance Locking (Reader-Writer Lock implementation) </h1>
 *
 * This is an implementation of the basic Double Instance Locking pattern with
 * two instances, using pthreads for the mutex, the reader-writer locks and the
 * thread-local variables.
 *
 * You can easily modify this to use "pass by reference" instead of the
 * thread-local (implemented as pthread_key_*).
 *
 *
 * This is used in pretty much the same way as a Reader-Writer Lock but
 * with a call to di_rwlock_wrtoggle() before applying the same mutation on the
 * opposite object (or data structure). Here's how you would do it:
 *
 * - Read-only operation:
 *     void * instance = di_rwlock_rdlock(&dirw);
 *     readOnlyFunction(instance);
 *     di_rwlock_rdunlock(&dirw);
 *
 * - Mutative operation:
 *    void * instance = di_rwlock_wrlock(&dirw);
 *    mutativeFunction(instance);
 *    instance = di_rwlock_wrtoggle(&dirw);
 *    mutativeFunction(instance);
 *    di_rwlock_wrunlock(&dirw);
 *
 */

#include <stdio.h>
#include <errno.h>
#include "di_rwlock.h"


/**
 * Initializes the Double Instance Lock
 * You can pass NULL as instance1 and instance2 if you want to handle the
 * instances yourself.
 *
 * @return If successful, returns 0, otherwise an error number
 */
int di_rwlock_init(di_rwlock_t * self, void *instance1, void *instance2) {
    int retval;
    if (dirw == NULL) return EINVAL;

    retval = pthread_mutex_init(&self->writers_mutex, NULL);
    if (retval != 0) return retval;
    retval += pthread_rwlock_init(&self->rwlock1, NULL);
    if (retval != 0) return retval;
    retval += pthread_rwlock_init(&self->rwlock2, NULL);
    if (retval != 0) return retval;
    retval = pthread_key_create(&self->key, NULL);
    if (retval != 0) return retval;

    self->instance1 = instance1;
    self->instance2 = instance2;
    return 0;
}


/**
 *
 */
int di_rwlock_destroy(di_rwlock_t * self) {
    int retval;
    if (dirw == NULL) return EINVAL;

    retval = pthread_mutex_destroy(&self->writers_mutex);
    if (retval != 0) return retval;
    retval = pthread_rwlock_destroy(&self->rwlock1);
    if (retval != 0) return retval;
    retval = pthread_rwlock_destroy(&self->rwlock2);
    if (retval != 0) return retval;
    retval = pthread_key_delete(self->key);
    if (retval != 0) return retval;

    self->instance1 = NULL;
    self->instance2 = NULL;
    return 0;
}


/**
 *
 */
void *di_rwlock_rdlock(di_rwlock_t * self) {
    while (1) {
        if (pthread_rwlock_tryrdlock(&self->rwlock1) == 0) {
            (void) pthread_setspecific(self->key, &self->rwlock1);
            return self->instance1;
        }
        if (pthread_rwlock_tryrdlock(&self->rwlock2) == 0) {
            (void) pthread_setspecific(self->key, &self->rwlock2);
            return self->instance2;
        }
    }
    return NULL;
}


/**
 *
 */
void di_rwlock_rdunlock(di_rwlock_t * self) {
    pthread_rwlock_t *held_lock = pthread_getspecific(self->key);
    pthread_rwlock_unlock(held_lock);
}


/**
 *
 */
void *di_rwlock_rdlock_held(di_rwlock_t * self, pthread_rwlock_t *held_lock) {
    while (1) {
        if (pthread_rwlock_tryrdlock(&self->rwlock1) == 0) {
            *held_lock = self->rwlock1;
            return self->instance1;
        }
        if (pthread_rwlock_tryrdlock(&self->rwlock2) == 0) {
            *held_lock = self->rwlock2;
            return self->instance2;
        }
    }
}


/**
 *
 */
void di_rwlock_rdunlock_held(di_rwlock_t * self, pthread_rwlock_t *held_lock) {
    pthread_rwlock_unlock(held_lock);
}

/**
 *
 */
void *di_rwlock_wrlock(di_rwlock_t * self) {
    pthread_mutex_lock(&self->writers_mutex);
    pthread_rwlock_wrlock(&self->rwlock2);
    return self->instance2;
}


/**
 *
 */
void *di_rwlock_wrtoggle(di_rwlock_t * self) {
    pthread_rwlock_unlock(&self->rwlock2);
    pthread_rwlock_wrlock(&self->rwlock1);
    return self->instance1;
}


/**
 *
 */
void di_rwlock_wrunlock(di_rwlock_t * self) {
    pthread_rwlock_unlock(&self->rwlock1);
    pthread_mutex_unlock(&self->writers_mutex);
}

