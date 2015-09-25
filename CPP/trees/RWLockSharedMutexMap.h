/******************************************************************************
 * Copyright (c) 2015, Pedro Ramalhete, Andreia Correia
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

#ifndef _RWLOCK_SHARED_MUTEX_MAP_H_
#define _RWLOCK_SHARED_MUTEX_MAP_H_

#include <map>
#include <iterator>
#include <shared_mutex>

/**
 * A std::map protected with a Left-Right Classic variant, using
 * Atomic Counters as ReadIndicators by default.
 *
 * We used the Left-Right pattern described in:
 http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */

template<typename Key, typename Value>
class RWLockSharedMutexMap {

private:
    std::shared_timed_mutex _rwlock;
    std::map<Key,Value>     _map;

public:
    // Default ReadIndicator is an Atomic Counter
    RWLockSharedMutexMap() {
    }

    ~RWLockSharedMutexMap() {
        _map.clear();
    }


    auto find( const Key& key ) {
        std::shared_lock<std::shared_timed_mutex> shar(_rwlock);
        return _map.find(key);
    }


    auto size() const {
        std::shared_lock<std::shared_timed_mutex> shar(_rwlock);
        return _map.size();
    }


    auto insert ( std::pair<Key,Value> val ) {
        std::unique_lock<std::shared_timed_mutex> excl(_rwlock);
        return _map.insert(val);
    }


    auto erase( const Key& key ) {
        std::unique_lock<std::shared_timed_mutex> excl(_rwlock);
        return _map.erase(key);
    }
};

#endif
