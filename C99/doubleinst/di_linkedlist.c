//copyright

/*
 * This code is compilable with a C99 compiler, i.e. pretty much any gcc version
 */
#include <stdio.h>
#include <stdlib.h>
#include "di_linkedlist.h"


// TODO: change self references to "const" if possible
// TODO: Provide Equals() in initializer


void di_linkedlist_init(di_linkedlist_t * const self) {
    pthread_mutex_init(&self->writersMutex, NULL);
    pthread_rwlock_init(&self->rwlock1, NULL);
    pthread_rwlock_init(&self->rwlock2, NULL);
    self->head1 = (dill_node_t *)malloc(sizeof(dill_node_t));
    self->head1->next1 = NULL;
    self->head1->next2 = NULL;
    self->head1->item = NULL;
    self->tail1 = self->head1;
    self->head2 = self->head1;
    self->tail2 = self->head1;
}


void di_linkedlist_destroy(di_linkedlist_t * const self) {
    pthread_mutex_destroy(&self->writersMutex);
    pthread_rwlock_destroy(&self->rwlock1);
    pthread_rwlock_destroy(&self->rwlock2);
    // TODO: walk the list and free all nodes
}



int di_linkedlist_add(di_linkedlist_t * const self, void * const item) {

    dill_node_t * newNode = (dill_node_t *)malloc(sizeof(dill_node_t));
    newNode->next1 = NULL;
    newNode->next2 = NULL;
    newNode->item = item;

    pthread_mutex_lock(&self->writersMutex);

    // Add item to linked list 2
    pthread_rwlock_wrlock(&self->rwlock2);
    self->tail2->next2 = newNode;
    self->tail2 = newNode;
    pthread_rwlock_unlock(&self->rwlock2);

    // Add item to linked list 1
    pthread_rwlock_wrlock(&self->rwlock1);
    self->tail1->next1 = newNode;
    self->tail1 = newNode;
    pthread_rwlock_unlock(&self->rwlock1);

    pthread_mutex_unlock(&self->writersMutex);

    return 0;
}



/**
 * Returns 1 if remove was successful, and 0 if item was not found
 */
int di_linkedlist_remove(di_linkedlist_t * const self, void * const item) {
    int retValue = 0;
    dill_node_t * node;
    dill_node_t * prev;
    pthread_mutex_lock(&self->writersMutex);

    // Remove item from linked list 2
    pthread_rwlock_wrlock(&self->rwlock2);
    node = self->head2->next2;
    prev = self->head2;
    while (node != NULL) {
        if (node->item == item) {
            prev->next2 = node->next2;
            retValue = 1;
            break;
        }
        prev = node;
        node = node->next2;
    }
    pthread_rwlock_unlock(&self->rwlock2);
    if (retValue == 0) {
        // The item is not in the linked list
        pthread_mutex_unlock(&self->writersMutex);
        return retValue;
    }

    // Remove item from linked list 1
    pthread_rwlock_wrlock(&self->rwlock1);
    prev->next1 = node->next1;
    free(node);
    pthread_rwlock_unlock(&self->rwlock1);

    pthread_mutex_unlock(&self->writersMutex);
    return retValue;
}


/**
 * Returns 1 if item is found, and zero if not found
 */
int di_linkedlist_contains(di_linkedlist_t * const self, void * const item) {
    dill_node_t * node;
    while (1) {
        if (pthread_rwlock_tryrdlock(&self->rwlock1) == 0) {
            node = self->head1->next1;
            while (node != NULL) {
                if (node->item == item) {
                    pthread_rwlock_unlock(&self->rwlock1);
                    return 1;
                }
                node = node->next1;
            }
            pthread_rwlock_unlock(&self->rwlock1);
            return 0;
        }

        if (pthread_rwlock_tryrdlock(&self->rwlock2) == 0) {
            node = self->head2->next2;
            while (node != NULL) {
                if (node->item == item) {
                    pthread_rwlock_unlock(&self->rwlock2);
                    return 1;
                }
                node = node->next2;
            }
            pthread_rwlock_unlock(&self->rwlock2);
            return 0;
        }
    }
}



