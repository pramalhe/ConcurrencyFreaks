//copyright

/*
 * This code is compilable with a C99 compiler, i.e. pretty much any gcc version
 */
#include <stdio.h>
#include <stdlib.h>
#include "rw_arraylist.h"


// TODO: change self references to "const" if possible
// TODO: Provide Equals() in initializer
// TODO: explain that resize occurs in powers of two, or pass min/max size of "buffer" elements
// TODO: implement growht() and shrink()
// TODO: use memmove() for add() e remove()

void rw_arraylist_init(rw_arraylist_t * self, int initialSize) {
    pthread_rwlock_init(&self->rwlock, NULL);
    self->arraySize = initialSize;
    self->usedSize = 0;
    self->arrayList = (void **)malloc(sizeof(void *)*self->arraySize);
}


void rw_arraylist_destroy(rw_arraylist_t * self) {
    pthread_rwlock_destroy(&self->rwlock);
    free(self->arrayList);
}



int rw_arraylist_add(rw_arraylist_t * self, void * item) {
    // Add item to arraylist
    pthread_rwlock_wrlock(&self->rwlock);
    //if (self->usedSize + 1 > self->arraySize) grow1and2andfree2();
    self->arrayList[self->usedSize] = item;
    self->usedSize++;
    pthread_rwlock_unlock(&self->rwlock);
    return 0;
}



// TODO: resize to shrink
/**
 * Returns 1 if the item was found and removed, 0 if there is no matching item
 */
int rw_arraylist_remove(rw_arraylist_t * self, void * item) {
    int i, index = -1;
    // Remove item from arraylist
    pthread_rwlock_wrlock(&self->rwlock);
    for (index = 0; index < self->usedSize; index++) {
        if (self->arrayList[index] == item) {
            // Move items to the right to fill empty slot
            for (i = index; i < self->usedSize-1; i++) {
                self->arrayList[i] = self->arrayList[i+1];
            }
            self->usedSize--;
            pthread_rwlock_unlock(&self->rwlock);
            return 1;
        }
    }
    pthread_rwlock_unlock(&self->rwlock);
    return 0;
}


/**
 * Returns 1 if item is found, and zero if not found
 */
int rw_arraylist_contains(rw_arraylist_t * self, void * item) {
    int index;
    pthread_rwlock_rdlock(&self->rwlock);
    for (index = 0; index < self->usedSize; index++) {
        if (self->arrayList[index] == item) {
            pthread_rwlock_unlock(&self->rwlock);
            return 1;
        }
    }
    pthread_rwlock_unlock(&self->rwlock);
    return 0;
}



