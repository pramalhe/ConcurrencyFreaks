//copyright

/*
 * This code is compilable with a C99 compiler, i.e. pretty much any gcc version
 */
#include <stdio.h>
#include <stdlib.h>
#include "rw_linkedlist.h"


// TODO: change self references to "const" if possible
// TODO: Provide Equals() in initializer


void rw_linkedlist_init(rw_linkedlist_t * self) {
    pthread_rwlock_init(&self->rwlock, NULL);
    self->head = (rwll_node_t *)malloc(sizeof(rwll_node_t));
    self->head->next = NULL;
    self->head->item = NULL;
    self->tail = self->head;
}


void rw_linkedlist_destroy(rw_linkedlist_t * self) {
    pthread_rwlock_destroy(&self->rwlock);
    // TODO: walk the list and free all nodes
}



int rw_linkedlist_add(rw_linkedlist_t * self, void * item) {
    rwll_node_t * newNode = (rwll_node_t *)malloc(sizeof(rwll_node_t));
    newNode->next = NULL;
    newNode->item = item;

    // Add item to linked list
    pthread_rwlock_wrlock(&self->rwlock);
    self->tail->next = newNode;
    self->tail = newNode;
    pthread_rwlock_unlock(&self->rwlock);

    return 0;
}



/**
 * Returns 1 if remove was successful, and 0 if item was not found
 */
int rw_linkedlist_remove(rw_linkedlist_t * self, void * item) {
    rwll_node_t * node;
    rwll_node_t * prev;

    // Remove item from linked list
    pthread_rwlock_wrlock(&self->rwlock);
    node = self->head->next;
    prev = self->head;
    while (node != NULL) {
        if (node->item == item) {
            prev->next = node->next;
            pthread_rwlock_unlock(&self->rwlock);
            return 1;
        }
        prev = node;
        node = node->next;
    }
    pthread_rwlock_unlock(&self->rwlock);
    return 0;
}


/**
 * Returns 1 if item is found, and zero if not found
 */
int rw_linkedlist_contains(rw_linkedlist_t * self, void * item) {
    rwll_node_t * node;
    pthread_rwlock_rdlock(&self->rwlock);
    node = self->head->next;
    while (node != NULL) {
        if (node->item == item) {
            pthread_rwlock_unlock(&self->rwlock);
            return 1;
        }
        node = node->next;
    }
    pthread_rwlock_unlock(&self->rwlock);
    return 0;
}



