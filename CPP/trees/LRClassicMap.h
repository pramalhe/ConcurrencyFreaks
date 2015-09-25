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

#ifndef _LEFT_RIGHT_CLASSIC_MAP_H_
#define _LEFT_RIGHT_CLASSIC_MAP_H_

#include <map>
#include <iterator>
#include <atomic>
#include "LeftRightClassic.h"
#include "RIAtomicCounter.h"

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

template<typename Key, typename Value, class RI = RIAtomicCounter>
class LRClassicMap {

private:
    static const int READS_ON_LEFT=0;
    static const int READS_ON_RIGHT=1;

    LeftRight::LeftRightClassic<RI> *_lrc       __attribute__(( aligned(64) ));
    std::atomic<int>                 _leftRight __attribute__(( aligned(64) )) { READS_ON_LEFT };
    std::map<Key,Value>              _mapLeft   __attribute__(( aligned(64) ));
    std::map<Key,Value>              _mapRight  __attribute__(( aligned(64) ));

public:
    // Default ReadIndicator is an Atomic Counter
    LRClassicMap() {
        _lrc = new LeftRight::LeftRightClassic<RI>();
    }

    ~LRClassicMap() {
        _mapLeft.clear();
        _mapRight.clear();
        delete _lrc;
    }


    auto find( const Key& key ) {
        const int lvi = _lrc->arrive();
        if (_leftRight.load() == READS_ON_LEFT) {
            auto ret = _mapLeft.find(key);
            _lrc->depart(lvi);
            return ret;
        } else {
            auto ret = _mapRight.find(key);
            _lrc->depart(lvi);
            return ret;
        }
    }


    auto size() const {
        const int lvi = _lrc->arrive();
        if (_leftRight.load() == READS_ON_LEFT) {
            auto ret = _mapLeft.size();
            _lrc->depart(lvi);
            return ret;
        } else {
            auto ret = _mapRight.size();
            _lrc->depart(lvi);
            return ret;
        }
    }


    // TODO: don't use auto here, use whatever it should be instead
    //int insert (const Value& val) {
    auto insert ( std::pair<Key,Value> val ) {
        _lrc->writersLock();
        if (_leftRight.load(std::memory_order_relaxed) == READS_ON_LEFT) {
            _mapRight.insert(val);
            _leftRight.store(READS_ON_RIGHT);
            _lrc->toggleVersionAndWait();
            auto ret = _mapLeft.insert(val);
            _lrc->writersUnlock();
            return ret;
        } else {
            _mapLeft.insert(val);
            _leftRight.store(READS_ON_LEFT);
            _lrc->toggleVersionAndWait();
            auto ret = _mapRight.insert(val);
            _lrc->writersUnlock();
            return ret;
        }
    }


    //auto erase( const Key& key ) {
    auto erase( const Key& key ) {
        _lrc->writersLock();
        if (_leftRight.load(std::memory_order_relaxed) == READS_ON_LEFT) {
            _mapRight.erase(key);
            _leftRight.store(READS_ON_RIGHT);
            _lrc->toggleVersionAndWait();
            auto ret = _mapLeft.erase(key);
            _lrc->writersUnlock();
            return ret;
        } else {
            _mapLeft.erase(key);
            _leftRight.store(READS_ON_LEFT);
            _lrc->toggleVersionAndWait();
            auto ret = _mapRight.erase(key);
            _lrc->writersUnlock();
            return ret;
        }
    }
};

#endif
