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


#ifndef _DCLC_RW_LOCK_LINKED_LIST_H_
#define _DCLC_RW_LOCK_LINKED_LIST_H_

#include "DCLCRWLock.h"
#include "LinkedListSet.h"


/**
 * A Linked List protected with a single global RW-Lock, in this case, an
 * implementation of C-RW-WP with DistributedCacheLinecounters => DCLCRWLock
 * <p>
 * More info on C-RW-WP with DCLC readIndicator in this blog post:
 * http://concurrencyfreaks.com/2013/09/distributed-cache-line-counter-scalable.html
 * or this paper just about C-RW-WP:
 * https://blogs.oracle.com/dave/resource/ppopp13-dice-NUMAAwareRWLocks.pdf
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Blocking
 * <li>remove(x)   - Blocking
 * <li>contains(x) - Blocking
 * </ul><p>
 */
template<typename T> class RWLockLinkedListDCLC {

private:
    LinkedListSet<T> _set;
    DCLCRWLock       _rwlock;

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
        _rwlock.exclusiveLock();
        bool retValue = _set.add(key);
        _rwlock.exclusiveUnlock();
        return retValue;
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
        _rwlock.exclusiveLock();
        bool retValue = _set.remove(key);
        _rwlock.exclusiveUnlock();
        return retValue;
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
        _rwlock.sharedLock();
        bool retValue = _set.contains(key);
        _rwlock.sharedUnlock();
        return retValue;
    }

};

#endif /* _DCLC_RW_LOCK_LINKED_LIST_H_ */
