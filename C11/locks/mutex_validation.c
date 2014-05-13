/******************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete, Andreia Correia
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

/*
 * This file can be compiled with something like (you'll need gcc 4.9.x):
 * gcc --std=c11 mutex_validation.c mpsc_mutex.c -lpthread -o mpsc
 * Feel free to add  -O3 -march=native
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>      /* Needed by sleep() */
#include <time.h>        /* Needed by rand()/srand() */
#include "mpsc_mutex.h"
#include "ticket_mutex.h"


/*
 * Benchmark parameters
 */
#define ARRAY_SIZE   (256)
#define NUM_THREADS  4


int *array1;

pthread_mutex_t pmutex;
mpsc_mutex_t mpscmutex;
ticket_mutex_t ticketmutex;

#define TYPE_PTHREAD_MUTEX   0
#define TYPE_MPSC_MUTEX      1
#define TYPE_TICKET_MUTEX    2

int g_which_lock = TYPE_PTHREAD_MUTEX;
int g_quit = 0;
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

    while (!g_quit) {
        if (g_which_lock == TYPE_PTHREAD_MUTEX) {
            /* Critical path for pthread_rwlock_t */
            pthread_mutex_lock(&pmutex);
            for (i = 0; i < ARRAY_SIZE; i++) array1[i]++;
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            pthread_mutex_unlock(&pmutex);
        } else if (g_which_lock == TYPE_MPSC_MUTEX) {
            /* Critical path for mpsc_mutex_t */
            mpsc_mutex_lock(&mpscmutex);
            for (i = 0; i < ARRAY_SIZE; i++) array1[i]++;
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            mpsc_mutex_unlock(&mpscmutex);
        } else {
            /* Critical path for ticket_mutex_t */
            ticket_mutex_lock(&ticketmutex);
            for (i = 0; i < ARRAY_SIZE; i++) array1[i]++;
            for (i = 1; i < ARRAY_SIZE; i++) {
                if (array1[i] != array1[0]) printf("ERROR\n");
            }
            ticket_mutex_unlock(&ticketmutex);
        }
        iterations++;
    }

    printf("Thread %d, iterations = %ld\n", *tid, iterations);
    g_operCounters[*tid] = iterations;
}


/**
 * Starts 4 pthreads and uses either a pthread_rwlock_t or a di_rwlock_t
 * to protect access to an array.
 *
 */
int main(void) {
    int i;
    pthread_t *pthread_list;
    int threadid[NUM_THREADS];

    /* Allocate memory for the two instance arrays */
    array1 = (int *)malloc(ARRAY_SIZE*sizeof(int));
    if (array1 == NULL) {
        printf("Not enough memory to allocate array\n");
        return -1;
    }
    for (i = 0; i < ARRAY_SIZE; i++) {
        array1[i] = 0;
    }

    /* Initialize locks */
    pthread_mutex_init(&pmutex, NULL);
    mpsc_mutex_init(&mpscmutex);
    ticket_mutex_init(&ticketmutex);

    printf("Starting benchmark with %d threads\n", NUM_THREADS);
    printf("Array has size of %d\n", ARRAY_SIZE);

    // Create the threads
    pthread_list = (pthread_t *)calloc(sizeof(pthread_t), NUM_THREADS);

    printf("Doing test for pthread_mutex_t, sleeping for 10 seconds...\n");
    g_which_lock = TYPE_PTHREAD_MUTEX;
    clearOperCounters();
    // Start the threads
    for(i = 0; i < NUM_THREADS; i++ ) {
        threadid[i] = i;
        pthread_create(&pthread_list[i], NULL, (void *(*)(void *))worker_thread, (void *)&threadid[i]);
    }
    sleep(10);
    g_quit = 1;

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(pthread_list[i], NULL);
    }
    g_quit = 0;
    printOperationsPerSecond();

    printf("Doing test for mpsc_mutex_t, sleeping for 10 seconds\n");
    g_which_lock = TYPE_MPSC_MUTEX;
    clearOperCounters();
    /* Start threads again, this time for the di_rwlock_t */
    for(i = 0; i < NUM_THREADS; i++ ) {
        pthread_create(&pthread_list[i], NULL, (void *(*)(void *))worker_thread, (void *)&threadid[i]);
    }
    sleep(10);
    g_quit = 1;
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(pthread_list[i], NULL);
    }
    g_quit = 0;
    printOperationsPerSecond();

    printf("Doing test for ticket_mutex_t, sleeping for 10 seconds...\n");
    g_which_lock = TYPE_TICKET_MUTEX;
    clearOperCounters();
    // Start the threads
    for(i = 0; i < NUM_THREADS; i++ ) {
        threadid[i] = i;
        pthread_create(&pthread_list[i], NULL, (void *(*)(void *))worker_thread, (void *)&threadid[i]);
    }
    sleep(10);
    g_quit = 1;
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(pthread_list[i], NULL);
    }
    g_quit = 0;
    printOperationsPerSecond();

    /* Destroy locks */
    pthread_mutex_destroy(&pmutex);
    mpsc_mutex_destroy(&mpscmutex);
    ticket_mutex_destroy(&ticketmutex);

    /* Release memory for the array instances and threads */
    free(array1);
    free(pthread_list);
    return 0;
}
