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


/**
 * Default constructor
 *
 */
DCLCRWLock::DCLCRWLock ()
{
    int hw_cores = std::thread::hardware_concurrency();
    // Default number of cores assumes this machine has 8 cores
    if (hw_cores == 0) hw_cores = DCLC_NUMBER_OF_CORES;
    numCores = hw_cores;
    countersLength = numCores*DCLC_COUNTERS_RATIO;
    writersMutex.store(DCLC_RWL_UNLOCKED);
    readersCounters = new std::atomic<int>[countersLength];
    for (int idx = 0; idx < countersLength; idx += DCLC_COUNTERS_RATIO) {
        readersCounters[idx] = 0;
    }
}


/**
 * With this constructor the use can specify the number of cores on the system
 */
DCLCRWLock::DCLCRWLock (int num_cores)
{
    this->numCores = num_cores;
    countersLength = num_cores*DCLC_COUNTERS_RATIO;
    writersMutex.store(DCLC_RWL_UNLOCKED);
    readersCounters = new std::atomic<int>[countersLength];
    for (int idx = 0; idx < countersLength; idx += DCLC_COUNTERS_RATIO) {
        readersCounters[idx] = 0;
    }
}


/**
 * Default destructor
 */
DCLCRWLock::~DCLCRWLock ()
{
    delete[] readersCounters;
    writersMutex.store(DCLC_RWL_UNLOCKED);
}


/**
 * Hashes a number and returns the index in the array of Reader's counters
 *
 * Returns a random index to be used in readers_counters[]
 */
int DCLCRWLock::thread2idx (void) {
    std::size_t tid = hashFunc(std::this_thread::get_id());
    return (int)((tid % numCores)*DCLC_COUNTERS_RATIO);
}


/**
 *
 */
void DCLCRWLock::sharedLock (void)
{
    const int idx = thread2idx();
    while (true) {
        readersCounters[idx].fetch_add(1);
        if (writersMutex.load() == DCLC_RWL_UNLOCKED) {
            // Acquired lock in read-only mode
            return;
        } else {
            // A Writer has acquired the lock, must reset to 0 and wait
            readersCounters[idx].fetch_add(-1);
            while (writersMutex.load() == DCLC_RWL_LOCKED) {
                std::this_thread::yield();
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
    if (readersCounters[thread2idx()].fetch_add(-1) <= 0) {
        // ERROR: no matching lock() for this unlock()
        std::cout << "ERROR: no matching lock() for this unlock()\n";
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
    while (!writersMutex.compare_exchange_strong(old, DCLC_RWL_LOCKED)) {
        std::this_thread::yield();
        old = DCLC_RWL_UNLOCKED;
    }
    // Write-lock was acquired, now wait for any running Readers to finish
    for (int idx = 0; idx < countersLength; idx += DCLC_COUNTERS_RATIO) {
        while (readersCounters[idx].load() > 0) {
            std::this_thread::yield();
        }
    }
}


/**
 *
 */
bool DCLCRWLock::exclusiveUnlock (void)
{
    if (writersMutex.load(std::memory_order_relaxed) != DCLC_RWL_LOCKED) {
        // ERROR: Tried to unlock an non-locked write-lock
        std::cout << "********* ERROR: Tried to unlock an non-locked write-lock\n";
        return false;
    }
    writersMutex.store(DCLC_RWL_UNLOCKED);
    return true;
}


bool DCLCRWLock::trySharedLock (void)
{
    const int tid = thread2idx();
    readersCounters[tid].fetch_add(1);
    if (writersMutex.load() == DCLC_RWL_UNLOCKED) {
        // Acquired lock in read-only mode
        return true;
    } else {
        // A Writer has acquired the lock, must reset to 0 and wait
        readersCounters[tid].fetch_add(-1);
        return false;
    }
}

bool DCLCRWLock::tryExclusiveLock (void)
{
    int old = DCLC_RWL_UNLOCKED;
    // Try to acquire the write-lock
    if (!writersMutex.compare_exchange_strong(old, DCLC_RWL_LOCKED)) {
        return false;
    }

    // Write-lock was acquired, now check for any running Readers
    for (int idx = 0; idx < countersLength; idx += DCLC_COUNTERS_RATIO) {
        if (readersCounters[idx].load() > 0) {
            writersMutex.store(DCLC_RWL_UNLOCKED);
            return false;
        }
    }
    return true;
}

bool DCLCRWLock::downgradeLock(void) {
    const int idx = thread2idx();
    readersCounters[idx].fetch_add(1);
    if (writersMutex.load(std::memory_order_relaxed) != DCLC_RWL_LOCKED) {
        // ERROR: Tried to downgrade an non-locked write-lock
        std::cout << "********* ERROR: Tried to downgrade an non-locked write-lock\n";
        return false;
    }
    writersMutex.store(DCLC_RWL_UNLOCKED);
    return true;
}

bool DCLCRWLock::tryUpgradeLock(void) {
    int old = DCLC_RWL_UNLOCKED;
    // Try to acquire the Write-lock
    if (!writersMutex.compare_exchange_strong(old, DCLC_RWL_LOCKED)) {
        return false;
    }

    if (readersCounters[thread2idx()].fetch_add(-1) <= 0) {
        // ERROR: no matching lock() for this upgrade()
        std::cout << "ERROR: no matching lock() for this upgrade()\n";
        // Release the Write-lock
        writersMutex.store(DCLC_RWL_UNLOCKED);
        return false;
    }

    // Write-lock was acquired, now check for any running Readers
    for (int idx = 0; idx < countersLength; idx += DCLC_COUNTERS_RATIO) {
        if (readersCounters[idx].load() > 0) {
            // Re-acquire the Read-lock and release the Write-lock
            readersCounters[thread2idx()].fetch_add(1);
            writersMutex.store(DCLC_RWL_UNLOCKED);
            return false;
        }
    }
    return true;
}
