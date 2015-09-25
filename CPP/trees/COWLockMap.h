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

#ifndef _COW_LOCK_MAP_H_
#define _COW_LOCK_MAP_H_

#include <map>
#include <iterator>
#include <atomic>
#include "LeftRightClassic.h"

/**
 * A std::map protected with a COWLock, using for memory reclamation a
 * Left-Right Classic variant with Atomic Counters as ReadIndicators.
 *
 * COWLock is described here:
 * https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/cowmq-2015.pdf
 *
 * We used the Left-Right pattern described in:
 http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */

template<typename Key, typename Value, class RI = RIAtomicCounter>
class COWLockMap {

private:
    LeftRight::LeftRightClassic<RI>    _lrc    __attribute__(( aligned(64) ));
    std::atomic<std::map<Key,Value>*>  _mapRef __attribute__(( aligned(64) ));

public:
    // Default ReadIndicator is an Atomic Counter
    COWLockMap() {
        _mapRef.store( new std::map<Key,Value>() );
    }


    ~COWLockMap() {
        _mapRef.load()->clear();
        delete _mapRef.load();
    }


    auto find( const Key& key ) {
        const int lvi = _lrc.arrive();
        auto ret = (_mapRef.load())->find(key);
        _lrc.depart(lvi);
        return ret;
    }


    auto size() {
        const int lvi = _lrc.arrive();
        auto ret = (_mapRef.load())->size();
        _lrc.depart(lvi);
        return ret;
    }


    auto insert ( std::pair<Key,Value> val ) {
        // We "highjack" the writersMutex from LR instead of using our own to protect _mapRef
        _lrc.writersLock();
        std::map<Key,Value> *currMap = _mapRef.load(std::memory_order_relaxed);
        std::map<Key,Value> *newMap = new std::map<Key,Value>(*currMap);
        auto ret = newMap->insert(val);
        _mapRef.store(newMap);
        // Wait for older Readers before deleting the old map
        _lrc.toggleVersionAndWait();
        _lrc.writersUnlock();
        currMap->clear(); // clear before deleting so we don't delete the keys/values
        delete currMap;
        return ret;
    }


    auto erase( const Key& key ) {
        // We "highjack" the writersMutex from LR instead of using our own to protect _mapRef
        _lrc.writersLock();
        std::map<Key,Value> *currMap = _mapRef.load(std::memory_order_relaxed);
        std::map<Key,Value> *newMap = new std::map<Key,Value>(*currMap);
        auto ret = newMap->erase(key);
        _mapRef.store(newMap);
        // Wait for older Readers before deleting the old map
        _lrc.toggleVersionAndWait();
        _lrc.writersUnlock();
        currMap->clear();
        delete currMap;
        return ret;
    }
};

#endif
