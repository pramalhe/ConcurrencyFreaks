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


#ifndef _RW_LOCK_LINKED_LIST_SHARED_MUTEX_H_
#define _RW_LOCK_LINKED_LIST_SHARED_MUTEX_H_

#include <shared_mutex>
#include "LinkedListSet.h"


/**
 * A Linked List protected with a single global RW-Lock, in this case, a
 * an std::shared_timed_mutex
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Blocking
 * <li>remove(x)   - Blocking
 * <li>contains(x) - Blocking
 * </ul><p>
 */
template<typename T>
class RWLockLinkedListSM {

private:
    LinkedListSet<T>        _set;
    std::shared_timed_mutex _rwlock;

public:

    /**
     * Adds a key to the set if it is not already there
     * <p>
     * Progress Condition: Blocking
     *
     * @param key
     * @return Returns true if "key" was added to the set, and false if it was
     * already there
     */
    bool add(T key) {
        std::unique_lock<std::shared_timed_mutex> excl(_rwlock);
        return _set.add(key);
    }


    /**
     * Removes a key from the set if it is there
     * <p>
     * Progress Condition: Blocking
     *
     * @param key
     * @return Returns true if "key" is found and removed from the list, false
     * if key was not found in the set.
     */
    bool remove(T key) {
        std::unique_lock<std::shared_timed_mutex> excl(_rwlock);
        return _set.remove(key);
    }


    /**
     * Searches for a given key.
     * <p>
     * Progress Condition: Blocking
     *
     * @param key
     * @return
     */
    bool contains(T key) {
        std::shared_lock<std::shared_timed_mutex> shar(_rwlock);
        return _set.contains(key);
    }

};

#endif /* _RW_LOCK_LINKED_LIST_SHARED_MUTEX_H_ */
