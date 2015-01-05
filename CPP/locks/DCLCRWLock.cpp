/******************************************************************************
 * Copyright (c) 2012-2015, Pedro Ramalhete, Andreia Correia
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


#include <thread>
#include <iostream>
#include <atomic>
#include "DCLCRWLock.h"

using namespace std;

/*
 * If you're using gcc 4.7 you might need this during compilation
 * _GLIBCXX_USE_NANOSLEEP to have std::this_thread::yield()
 */



/**
 * Default constructor
 *
 */
DCLCRWLock::DCLCRWLock ()
{
    int hw_cores = std::thread::hardware_concurrency();
    // Default number of cores assumes this machine has 8 cores
    if (hw_cores == 0) hw_cores = DCLC_NUMBER_OF_CORES;
    num_cores = hw_cores;
    counters_length = num_cores*DCLC_COUNTERS_RATIO;
    writers_mutex.store(DCLC_RWL_UNLOCKED);
    readers_counters = new atomic<int>[counters_length];
    for (int tid = 0; tid < counters_length; tid += DCLC_COUNTERS_RATIO) {
        readers_counters[tid] = 0;
    }
}


/**
 * With this constructor the use can specify the number of cores on the system
 */
DCLCRWLock::DCLCRWLock (int num_cores)
{
    this->num_cores = num_cores;
    counters_length = num_cores*DCLC_COUNTERS_RATIO;
    writers_mutex.store(DCLC_RWL_UNLOCKED);
    readers_counters = new atomic<int>[counters_length];
    for (int tid = 0; tid < counters_length; tid += DCLC_COUNTERS_RATIO) {
        readers_counters[tid] = 0;
    }
}


/**
 * Default destructor
 */
DCLCRWLock::~DCLCRWLock ()
{
    delete[] readers_counters;
    writers_mutex.store(DCLC_RWL_UNLOCKED);
}


/**
 * Hashes a number and returns the index in the array of Reader's counters
 *
 * Returns a random index to be used in readers_counters[]
 */
int DCLCRWLock::thread_2_tid (void) {
    std::hash<std::thread::id> hashFunc;
    std::size_t tid = hashFunc(std::this_thread::get_id());
    tid ^= (tid << 21);
    tid ^= (tid >> 35);
    tid ^= (tid << 4);
    return (int)((tid % num_cores)*DCLC_COUNTERS_RATIO);
}


/**
 *
 */
void DCLCRWLock::sharedLock (void)
{
    const int tid = thread_2_tid();
    while (1) {
        readers_counters[tid].fetch_add(1);
        if (writers_mutex.load() == DCLC_RWL_UNLOCKED) {
            // Acquired lock in read-only mode
            return;
        } else {
            // A Writer has acquired the lock, must reset to 0 and wait
            readers_counters[tid].fetch_add(-1);
            while (writers_mutex.load() == DCLC_RWL_LOCKED) {
                this_thread::yield();
            }
        }
    }
}


/**
 *
 *
 */
bool DCLCRWLock::sharedUnlock (void)
{
    const int tid = thread_2_tid();
    if (readers_counters[tid].fetch_add(-1) <= 0) {
        // ERROR: no matching lock() for this unlock()
        cout << "ERROR: no matching lock() for this unlock()\n";
        return false;
    } else {
        return true;
    }
}


/**
 *
 */
void DCLCRWLock::exclusiveLock (void)
{
    int old = DCLC_RWL_UNLOCKED;
    // Try to acquire the write-lock
    while (!writers_mutex.compare_exchange_strong(old, DCLC_RWL_LOCKED)) {
        this_thread::yield();
        old = DCLC_RWL_UNLOCKED;
    }
    // Write-lock was acquired, now wait for any running Readers to finish
    for (int tid = 0; tid < counters_length; tid += DCLC_COUNTERS_RATIO) {
        while (readers_counters[tid].load() > 0) {
            this_thread::yield();
        }
    }
}


/**
 *
 */
bool DCLCRWLock::exclusiveUnlock (void)
{
	if (writers_mutex.load(std::memory_order_relaxed) != DCLC_RWL_LOCKED) {
        // ERROR: Tried to unlock an non-locked write-lock
        cout << "********* ERROR: Tried to unlock an non-locked write-lock\n";
        return false;
    }
    writers_mutex.store(DCLC_RWL_UNLOCKED);
    return true;
}


bool DCLCRWLock::trySharedLock (void)
{
    const int tid = thread_2_tid();
    readers_counters[tid].fetch_add(1);
    if (writers_mutex.load() == DCLC_RWL_UNLOCKED) {
        // Acquired lock in read-only mode
        return true;
    } else {
        // A Writer has acquired the lock, must reset to 0 and wait
        readers_counters[tid].fetch_add(-1);
        return false;
    }
}

/* TODO: implement the writer's trylock and the timeouts */
