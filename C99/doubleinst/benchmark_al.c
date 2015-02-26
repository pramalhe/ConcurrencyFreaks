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
#include "di_arraylist.h"
#include "rw_arraylist.h"
#include "di_linkedlist.h"
#include "rw_linkedlist.h"
//#include "crwwp_linkedlist.h"

/*
 * Benchmark parameters
 */
#define ARRAY_SIZE   (1*1000)
#define NUM_THREADS  128

/*
 * Global variables
 */
int *array1;
int g_writePerMil = 0; // 10 is 1% and it works good on opteron 32 cores with 10k lists/arrays

di_arraylist_t dial;
rw_arraylist_t rwal;
di_linkedlist_t dill;
rw_linkedlist_t rwll;
//crwwp_linkedlist_t crwwpll;

#define TYPE_READER_WRITER_AL       0
#define TYPE_DOUBLE_INSTANCE_AL     1
#define TYPE_READER_WRITER_LL       2
#define TYPE_DOUBLE_INSTANCE_LL     3
#define TYPE_CRWWP_LL               4


atomic_int g_quit = ATOMIC_VAR_INIT(0);
// These two don't have to be atomic because they are set before the threads are created or read after the threads join
int g_which_lock = TYPE_READER_WRITER_AL;
int g_operCounters[NUM_THREADS];


static void clearOperCounters(void) {
    int i;
    for (i = 0; i < NUM_THREADS; i++) g_operCounters[i] = 0;
}

static void printOperationsPerSecond() {
    int i;
    long sum = 0;
    for (i = 0; i < NUM_THREADS; i++) sum += g_operCounters[i];
    printf("Operations/sec = %ld\n", sum);
}

static long long randomLong(long long x) {
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    return x * 2685821657736338717LL;
}


/**
 *
 */
void worker_thread(int *tid) {
    int x;
    long long xrand = 12345678901234567LL;
    long iterations = 0;
    void * item1;
    void * item2;

    while (!atomic_load(&g_quit)) {
        // Get random numbers for the probability of doing write operation or
        // read and for which items will be read/inserted/removed.
        xrand = randomLong(xrand);
        x = (xrand < 0) ? (-xrand)%1000 : xrand%1000;
        xrand = randomLong(xrand);
        item1 = &array1[(unsigned int)xrand % ARRAY_SIZE];
        xrand = randomLong(xrand);
        item2 = &array1[(unsigned int)xrand % ARRAY_SIZE];

        if (g_writePerMil != 1000 && (g_writePerMil == 0 || x >= g_writePerMil)) {
            if (g_which_lock == TYPE_READER_WRITER_AL) {
                rw_arraylist_contains(&rwal, item1);
                rw_arraylist_contains(&rwal, item2);
            } else if (g_which_lock == TYPE_DOUBLE_INSTANCE_AL) {
                di_arraylist_contains(&dial, item1);
                di_arraylist_contains(&dial, item2);
            } else if (g_which_lock == TYPE_READER_WRITER_LL) {
                rw_linkedlist_contains(&rwll, item1);
                rw_linkedlist_contains(&rwll, item2);
            } else if (g_which_lock == TYPE_DOUBLE_INSTANCE_LL) {
                di_linkedlist_contains(&dill, item1);
                di_linkedlist_contains(&dill, item2);
            } else if (g_which_lock == TYPE_CRWWP_LL) {
                //crwwp_linkedlist_contains(&crwwpll, item1);
                //crwwp_linkedlist_contains(&crwwpll, item2);
            }
        } else {
            if (g_which_lock == TYPE_READER_WRITER_AL) {
                rw_arraylist_remove(&rwal, item1);
                rw_arraylist_add(&rwal, item1);
            } else if (g_which_lock == TYPE_DOUBLE_INSTANCE_AL) {
                di_arraylist_remove(&dial, item1);
                di_arraylist_add(&dial, item1);
            } else if (g_which_lock == TYPE_READER_WRITER_LL) {
                rw_linkedlist_remove(&rwll, item1);
                rw_linkedlist_add(&rwll, item1);
            } else if (g_which_lock == TYPE_DOUBLE_INSTANCE_LL) {
                di_linkedlist_remove(&dill, item1);
                di_linkedlist_add(&dill, item1);
            } else if (g_which_lock == TYPE_CRWWP_LL) {
                //crwwp_linkedlist_remove(&crwwpll, item1);
                //crwwp_linkedlist_add(&crwwpll, item1);
            }
        }
        iterations++;
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
    //int threadList[] = { 1, 2, 4, 8, 16, 24, 32, 48, 64, 128 }; // size is 10
    int threadList[] = { 4, 1, 2, 8, 16, 24, 32, 48, 64, 128 }; // size is 10

    /* Allocate memory for the two instance arrays */
    array1 = (int *)malloc(ARRAY_SIZE*sizeof(int));
    if (array1 == NULL) {
        printf("Not enough memory to allocate array\n");
        return -1;
    }
    for (i = 0; i < ARRAY_SIZE; i++) array1[i] = i;

    /* Initialize arraylists and insert items into them */
    rw_arraylist_init(&rwal, 2*ARRAY_SIZE);
    di_arraylist_init(&dial, 2*ARRAY_SIZE);
    rw_linkedlist_init(&rwll);
    di_linkedlist_init(&dill);
    //crwwp_linkedlist_init(&crwwpll);
    for (i = 0; i < ARRAY_SIZE; i++) {
        rw_arraylist_add(&rwal, &array1[i]);
        di_arraylist_add(&dial, &array1[i]);
        rw_linkedlist_add(&rwll, &array1[i]);
        di_linkedlist_add(&dill, &array1[i]);
        //crwwp_linkedlist_add(&crwwpll, &array1[i]);
    }

    printf("Starting benchmark with %d threads\n", NUM_THREADS);
    printf("Array has size of %d\n", ARRAY_SIZE);

    // Create the threads
    pthread_list = (pthread_t *)calloc(sizeof(pthread_t), NUM_THREADS);

    for (i = 0; i < 10; i ++) {
        printf("\n---------- Active threads: %d ----------\n", threadList[i]);
        singleTest(threadList[i], "rw_arraylist_t, sleeping for 10 seconds...\n",   TYPE_READER_WRITER_AL,   pthread_list);
        singleTest(threadList[i], "di_arraylist, sleeping for 10 seconds...\n",     TYPE_DOUBLE_INSTANCE_AL, pthread_list);
        singleTest(threadList[i], "rw_linkedlist_t, sleeping for 10 seconds...\n",  TYPE_READER_WRITER_LL,   pthread_list);
        singleTest(threadList[i], "di_linkedlist, sleeping for 10 seconds...\n",    TYPE_DOUBLE_INSTANCE_LL, pthread_list);
        //singleTest(threadList[i], "crwwp_linkedlist, sleeping for 10 seconds...\n", TYPE_CRWWP_LL,           pthread_list);
    }

    /* Destroy arraylists */
    rw_arraylist_destroy(&rwal);
    di_arraylist_destroy(&dial);
    rw_linkedlist_destroy(&rwll);
    di_linkedlist_destroy(&dill);
    //crwwp_linkedlist_destroy(&crwwpll);

    /* Release memory for the array instances and threads */
    free(array1);
    free(pthread_list);
    return 0;
}
