/******************************************************************************
 * Copyright (c) 2012-2014, Pedro Ramalhete, Andreia Correia
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
#include "FAARWLock.h"

using namespace std;

/*
 * If you're using gcc 4.7 you might need this during compilation
 * _GLIBCXX_USE_NANOSLEEP to have std::this_thread::yield()
 */



/**
 * Default constructor
 *
 */
FAARWLock::FAARWLock ()
{
    writers_mutex.store(FAA_RWL_UNLOCKED);
    readers_count.store(0);
}


/**
 * Default destructor
 */
FAARWLock::~FAARWLock ()
{
	readers_count.store(0);
	writers_mutex.store(FAA_RWL_UNLOCKED);
}



void FAARWLock::sharedLock (void)
{
    while (1) {
    	readers_count.fetch_add(1);
		if (writers_mutex.load() == FAA_RWL_UNLOCKED) {
			// Acquired lock in read-only mode
			return;
		} else {
            // A Writer has acquired the lock, must reset to 0 and wait
			readers_count.fetch_add(-1);
            while (writers_mutex.load() == FAA_RWL_LOCKED) {
            	this_thread::yield();
            }
		}
    }
}


/**
 *
 *
 */
bool FAARWLock::sharedUnlock (void)
{
	if (readers_count.fetch_add(-1) <= 0) {
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
void FAARWLock::exclusiveLock (void)
{
	int old = FAA_RWL_UNLOCKED;
    // Try to acquire the write-lock
    while (!writers_mutex.compare_exchange_strong(old, FAA_RWL_LOCKED)) {
    	this_thread::yield();
    	old = FAA_RWL_UNLOCKED;
    }

    // Write-lock was acquired, now wait for any running Readers to finish
    while (readers_count.load() > 0) {
    	this_thread::yield();
    }
}


/**
 *
 */
bool FAARWLock::exclusiveUnlock (void)
{
	// TODO: use relaxed atomic?
	if (writers_mutex.load() != FAA_RWL_LOCKED) {
		// ERROR: Tried to unlock an non-locked write-lock
		cout << "********* ERROR: Tried to unlock an non-locked write-lock\n";
		return false;
	}
	writers_mutex.store(FAA_RWL_UNLOCKED);
	return true;
}

/* TODO: implement the trylocks and timeouts */
