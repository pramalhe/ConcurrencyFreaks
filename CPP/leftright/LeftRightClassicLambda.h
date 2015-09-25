/******************************************************************************
 * Copyright (c) 2014-2015, Pedro Ramalhete, Andreia Correia
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

#ifndef _LEFT_RIGHT_CLASSIC_LAMBDA_H_
#define _LEFT_RIGHT_CLASSIC_LAMBDA_H_

#include <atomic>
#include <mutex>
#include <functional>
#include "ReadIndicator.h"
#include "RIAtomicCounter.h"

namespace LeftRight {

/**
 * Left-Right with Lambdas - Classic variant
 * Can use any ReadIndicator, i.e. classes that implement sequentially
 * consistent arrive()/depart()/isEmpty(), like the ones in the folder
 * 'readindicators'.
 *
 * arrive()               - Progress of ReadIndicator.arrive()
 * depart()               - Progress of ReadIndicator.depart()
 * toggleVersionAndWait() - Blocking
 * writersLock()          - Blocking
 * witersUnlock()         - Wait-Free
 *
 * We used the Left-Right pattern described in:
 http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * <p>
 * For an example see LRClassicMap.h
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T, class RI = RIAtomicCounter>
class LeftRightClassicLambda {

private:
    static const int READS_LEFT = 0;
    static const int READS_RIGHT = 1;

    LeftRightClassic<RI> _lrc;
    std::atomic<int>     _leftRight { READS_LEFT };
    T*                   _leftInst = nullptr;    // TODO: const
    T*                   _rightInst = nullptr;   // TODO: const
    bool                 _outerAlloc { false };

public:

    LeftRightClassicLambda(T* leftInst, T* rightInst) {
        _leftInst = leftInst;
        _rightInst = rightInst;
        _outerAlloc = true;
    }

    LeftRightClassicLambda() {
        _leftInst = new T();
        _rightInst = new T();
    }

    ~LeftRightClassicLambda() {
        if (!_outerAlloc) {
            delete _leftInst;
            delete _rightInst;
        }
    }


    template<typename R, typename A>
    R applyRead(A& arg1, std::function<R(T*,A)>& readOnlyFunc) {
        const int lvi = _lrc.arrive();
        T* inst = _leftRight.load() == READS_LEFT ? _leftInst : _rightInst;
        R ret = readOnlyFunc(inst, arg1);
        _lrc.depart(lvi);
        return ret;
    }


    template<typename R, typename A>
    R applyMutation(A& arg1, std::function<R(T*,A)>& mutativeFunc) {
        std::lock_guard<std::mutex> lock(_lrc._writersMutex);
        if (_leftRight.load(std::memory_order_relaxed) == READS_LEFT) {
            mutativeFunc(_rightInst, arg1);
            _leftRight.store(READS_RIGHT);
            _lrc.toggleVersionAndWait();
            return mutativeFunc(_leftInst, arg1);
        } else {
            mutativeFunc(_leftInst, arg1);
            _leftRight.store(READS_LEFT);
            _lrc.toggleVersionAndWait();
            return mutativeFunc(_rightInst, arg1);
        }
    }
};
}

#endif /* _LEFT_RIGHT_CLASSIC_LAMBDA_H_ */
