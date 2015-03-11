/******************************************************************************
 * Copyright (c) 2014-2015, Pedro Ramalhete, Andreia Correia
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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>      /* Needed by sleep() */
#include <time.h>        /* Needed by rand()/srand() */
#include <stdatomic.h>
#include "ticket_mutex.h"
#include "tidex_mutex.h"

/*
 * Benchmark parameters
 */
#define ARRAY_SIZE   (256)
#define NUM_THREADS  128

/*
 * Global variables
 */
int *array1;

pthread_mutex_t pmutex;
ticket_mutex_t ticketmutex;
tidex_mutex_t tidexmutex;


#define TYPE_PTHREAD_MUTEX        0
#define TYPE_TICKET_MUTEX         3
#define TYPE_TIDEX_MUTEX          5


atomic_int g_quit = ATOMIC_VAR_INIT(0);
// These two don't have to be atomic because they are set before the threads are created or read after the threads join
int g_which_lock = TYPE_PTHREAD_MUTEX;
int g_operCounters[NUM_THREADS];


static void clearOperCounters(void) {
    int i;
    for (i = 0; i < NUM_THREADS; i++) g_operCounters[i] = 0;
}

static void printOperationsPerSecond() {
    int i;
    long sum = 0;
    for (i = 0; i < NUM_THREADS; i++) sum += g_operCounters[i];
    printf("Operations/sec = %d\n", sum);
}

/**
 *
 */
void worker_thread(int *tid) {
    int i;
    int *current_array;
    long iterations = 0;
    int ncarray[ARRAY_SIZE*10];

    for (i = 0; i < ARRAY_SIZE*10; i++) ncarray[i] = 99;

    while (!atomic_load(&g_quit)) {
        if (g_which_lock == TYPE_PTHREAD_MUTEX) {
            /* Critical path for pthread_rwlock_t */
            pthread_mutex_lock(&pmutex);
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            pthread_mutex_unlock(&pmutex);
        } else if (g_which_lock == TYPE_TICKET_MUTEX) {
            /* Critical path for ticket_mutex_t */
            ticket_mutex_lock(&ticketmutex);
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            ticket_mutex_unlock(&ticketmutex);
        } else  if (g_which_lock == TYPE_TIDEX_MUTEX) {
            /* Critical path for tidex_mutex_t */
            tidex_mutex_lock(&tidexmutex);
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            tidex_mutex_unlock(&tidexmutex);
        }
        iterations++;

        // Non-critical path, about 10x larger
        for (i = 1; i < ARRAY_SIZE*10; i++) {
            if (ncarray[i] != ncarray[0]) printf("ERROR\n");
        }
    }

    //printf("Thread %d, iterations = %ld\n", *tid, iterations);
    g_operCounters[*tid] = iterations;
}


/**
 * A single run
 */
void singleTest(int numThreads, char * title, int lock_type, pthread_t * pthread_list) {
    int i;
    int threadid[NUM_THREADS];
    printf(title);
    g_which_lock = lock_type;
    clearOperCounters();
    // Start the threads
    for(i = 0; i < numThreads; i++ ) {
        threadid[i] = i;
        pthread_create(&pthread_list[i], NULL, (void *(*)(void *))worker_thread, (void *)&threadid[i]);
    }
    sleep(10);
    atomic_store(&g_quit, 1);
    for (i = 0; i < numThreads; i++) {
        pthread_join(pthread_list[i], NULL);
    }
    atomic_store(&g_quit, 0);
    printOperationsPerSecond();
}


/**
 * Starts 4 pthreads and uses either a pthread_rwlock_t or a di_rwlock_t
 * to protect access to an array.
 *
 */
int main(void) {
    int i;
    pthread_t *pthread_list;
    int numThreads = 0;
    int threadList[] = { 1, 2, 4, 8, 16, 24, 32, 48, 64, 128 }; // size is 10

    /* Allocate memory for the two instance arrays */
    array1 = (int *)malloc(ARRAY_SIZE*sizeof(int));
    if (array1 == NULL) {
        printf("Not enough memory to allocate array\n");
        return -1;
    }
    for (i = 0; i < ARRAY_SIZE; i++) array1[i] = 0;

    /* Initialize locks */
    pthread_mutex_init(&pmutex, NULL);
    ticket_mutex_init(&ticketmutex);
    tidex_mutex_init(&tidexmutex);

    printf("Starting benchmark with %d threads\n", NUM_THREADS);
    printf("Array has size of %d\n", ARRAY_SIZE);

    // Create the threads
    pthread_list = (pthread_t *)calloc(sizeof(pthread_t), NUM_THREADS);


    for (int i = 0; i < 10; i ++) {
        printf("\n---------- Active threads: %d ----------\n", threadList[i]);
        singleTest(threadList[i], "pthread_mutex_t, sleeping for 10 seconds...\n", TYPE_PTHREAD_MUTEX, pthread_list);
        singleTest(threadList[i], "ticket_mutex_t, sleeping for 10 seconds...\n",  TYPE_TICKET_MUTEX,  pthread_list);
        singleTest(threadList[i], "tidex_mutex_t, sleeping for 10 seconds...\n",   TYPE_TIDEX_MUTEX,   pthread_list);
    }

    /* Destroy locks */
    pthread_mutex_destroy(&pmutex);
    ticket_mutex_destroy(&ticketmutex);
    tidex_mutex_destroy(&tidexmutex);

    /* Release memory for the array instances and threads */
    free(array1);
    free(pthread_list);
    return 0;
}
