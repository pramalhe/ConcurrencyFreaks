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

#ifndef _RWLOCK_PTHREAD_MAP_H_
#define _RWLOCK_PTHREAD_MAP_H_

#include <map>
#include <iterator>
#include <atomic>
#include <pthread.h>

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
class RWLockPthreadMap {

private:
    pthread_rwlock_t        _rwlock __attribute__(( aligned(64) )) = PTHREAD_RWLOCK_INITIALIZER;
    std::map<Key,Value>     _map    __attribute__(( aligned(64) ));

public:
    // Default ReadIndicator is an Atomic Counter
    RWLockPthreadMap() {
        pthread_rwlock_init(&_rwlock, NULL);
    }

    ~RWLockPthreadMap() {
        _map.clear();
        pthread_rwlock_destroy(&_rwlock);
    }


    auto find( const Key& key ) {
        pthread_rwlock_rdlock(&_rwlock);
        auto ret = _map.find(key);
        pthread_rwlock_unlock(&_rwlock);
        return ret;
    }


    auto size() const {
        pthread_rwlock_rdlock(&_rwlock);
        auto ret = _map.size();
        pthread_rwlock_unlock(&_rwlock);
        return ret;
    }


    //int insert (const Value& val) {
    auto insert ( std::pair<Key,Value> val ) {
        pthread_rwlock_wrlock(&_rwlock);
        auto ret = _map.insert(val);
        pthread_rwlock_unlock(&_rwlock);
        return ret;
    }


    //auto erase( const Key& key ) {
    auto erase( const Key& key ) {
        pthread_rwlock_wrlock(&_rwlock);
        auto ret = _map.erase(key);
        pthread_rwlock_unlock(&_rwlock);
        return ret;
    }
};

#endif
