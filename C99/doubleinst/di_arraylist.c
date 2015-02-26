//copyright

/*
 * This code is compilable with a C99 compiler, i.e. pretty much any gcc version
 */
#include <stdio.h>
#include <stdlib.h>
#include "di_arraylist.h"


// TODO: Provide Equals() in initializer
// TODO: explain that resize occurs in powers of two, or pass min/max size of "buffer" elements
// TODO: implement growht() and shrink()
// TODO: use memmove() for add() e remove()


void di_arraylist_init(di_arraylist_t * const self, int initialSize) {
    pthread_mutex_init(&self->writersMutex, NULL);
    pthread_rwlock_init(&self->rwlock1, NULL);
    pthread_rwlock_init(&self->rwlock2, NULL);
    self->arraySize = initialSize;
    self->usedSize1 = 0;
    self->usedSize2 = 0;
    self->arrayList1 = (void **)malloc(sizeof(void *)*self->arraySize);
    self->arrayList2 = (void **)malloc(sizeof(void *)*self->arraySize);
}


void di_arraylist_destroy(di_arraylist_t * const self) {
    pthread_mutex_destroy(&self->writersMutex);
    pthread_rwlock_destroy(&self->rwlock1);
    pthread_rwlock_destroy(&self->rwlock2);
    free(self->arrayList1);
    free(self->arrayList2);
}



int di_arraylist_add(di_arraylist_t * const self, void * const item) {
    pthread_mutex_lock(&self->writersMutex);

    // Add item to arraylist 2
    pthread_rwlock_wrlock(&self->rwlock2);
    //if (self->usedSize2 + 1 > self->arraySize) grow1and2andfree2();
    self->arrayList2[self->usedSize2] = item;
    self->usedSize2++;
    pthread_rwlock_unlock(&self->rwlock2);

    // Add item to arraylist 1
    pthread_rwlock_wrlock(&self->rwlock1);
    //assign1andfree1();
    self->arrayList1[self->usedSize1] = item;
    self->usedSize1++;
    pthread_rwlock_unlock(&self->rwlock1);

    pthread_mutex_unlock(&self->writersMutex);

    return 0;
}



// TODO: resize to shrink
int di_arraylist_remove(di_arraylist_t * const self, void * const item) {
    int i, index = -1;
    int retValue = 0;
    pthread_mutex_lock(&self->writersMutex);

    // Remove item from arraylist 2
    pthread_rwlock_wrlock(&self->rwlock2);
    for (index = 0; index < self->usedSize2; index++) {
        if (self->arrayList2[index] == item) {
            // Move items to the right to fill empty slot
            for (i = index; i < self->usedSize2-1; i++) {
                self->arrayList2[i] =self->arrayList2[i+1];
            }
            retValue = 1;
        }
    }
    if (retValue == 0) {
        // The item is not in the arraylist
        pthread_rwlock_unlock(&self->rwlock2);
        pthread_mutex_unlock(&self->writersMutex);
        return retValue;
    }
    self->usedSize2--;
    pthread_rwlock_unlock(&self->rwlock2);

    // Remove item from arraylist 1
    pthread_rwlock_wrlock(&self->rwlock1);
    // Move items to the right to fill empty slot
    for (i = index; i < self->usedSize1-1; i++) {
        self->arrayList1[i] =self->arrayList1[i+1];
    }
    self->usedSize1--;
    pthread_rwlock_unlock(&self->rwlock1);

    pthread_mutex_unlock(&self->writersMutex);
    return retValue;
}


/**
 * Returns 1 if item is found, and zero if not found
 */
int di_arraylist_contains(di_arraylist_t * const self, void * const item) {
    int index;
    while (1) {
        if (pthread_rwlock_tryrdlock(&self->rwlock1) == 0) {
            for (index = 0; index < self->usedSize1; index++) {
                if (self->arrayList1[index] == item) {
                    pthread_rwlock_unlock(&self->rwlock1);
                    return 1;
                }
            }
            pthread_rwlock_unlock(&self->rwlock1);
            return 0;
        }

        if (pthread_rwlock_tryrdlock(&self->rwlock2) == 0) {
            //printf("had to go into second instance\n");
            for (index = 0; index < self->usedSize2; index++) {
                if (self->arrayList2[index] == item) {
                    pthread_rwlock_unlock(&self->rwlock2);
                    return 1;
                }
            }
            pthread_rwlock_unlock(&self->rwlock2);
            return 0;
        }
    }
}



