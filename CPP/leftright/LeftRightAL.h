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

#ifndef _LEFT_RIGHT_ATOMIC_LONG_H_
#define _LEFT_RIGHT_ATOMIC_LONG_H_

#include <atomic>
#include <mutex>
#include <thread>

namespace LeftRight {

static const long READS_ON_LEFT  = 0;
static const long READS_ON_RIGHT = 1;


/**
 * Left-Right Atomic Long Classic variant
 * Uses a single atomic long for the two counters and leftRight and
 * versionIndex variables.
 *
 * arrive()               - Wait-Free (on x86)
 * depart()               - Wait-Free (on x86)
 * toggleVersionAndWait() - Blocking
 * writersLock()          - Blocking
 * witersUnlock()         - Wait-Free
 *
 * We used the Left-Right pattern described in:
 http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * with the Atomic Long variant implementation described in:
 * http:...
 * <p>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LeftRightAL {

private:
    // Position of contents of leftRightCounters
    static const int BIT_RI0          = 0;
    static const int BIT_RI1          = 30;
    static const int BIT_LEFTRIGHT    = 60;
    static const int BIT_VERSIONINDEX = 61;

    // Bitmask of a counter (30 bits)
    static const long long MASK_COUNTER = (1LL << 30) - 1LL;

    // Members used by the Left-Right mechanism
    std::atomic<long long>  _rivilr; // ReadIndicators + versionIndex + leftRight
    std::mutex              _writersMutex;

public:
    // Yes, we want these public
    //T _instanceLeft;
    //T _instanceRight;

    LeftRightAL() {
        _rivilr.store(composeRILRVI(0, 0, 0, READS_ON_LEFT));
    }

    /*
    LeftRightAL(T linst, T rinst) {
        _instanceLeft = linst;
        _instanceRight = rinst;
        _rivilr.store(composeRILRVI(0, 0, 0, READS_ON_LEFT));
    }
    */


    /**
     * Marks that a new Reader has arrived at the readIndicator.
     *
     * Progress Condition: Wait-Free Population Oblivious (on x86)
     *
     * @return the current versionIndex
     */
    int arrive(void) {
        const int localVersionIndex = getVersionIndex(_rivilr.load());
        if (localVersionIndex == 0) {
            _rivilr.fetch_add(1LL << BIT_RI0);
        } else {
            _rivilr.fetch_add(1LL << BIT_RI1);
        }
        return localVersionIndex;
    }


    /**
     * Marks that a Reader has departed from the readIndicator.
     *
     * Progress Condition: Wait-Free Population Oblivious (on x86)
     *
     * @param localVersionIndex Pass the value returned by arrive()
     */
    void depart(int localVersionIndex) {
        if (localVersionIndex == 0) {
            _rivilr.fetch_add(-(1LL << BIT_RI0));
        } else {
            _rivilr.fetch_add(-(1LL << BIT_RI1));
        }
    }


    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the
     * Set that the "Writer" wants to modify.
     * Must be called only by "Writer" operations, and {@code writersMutex}
     * must be locked when this function is called.
     *
     * Although this thread is the only one modifying the versionIndex or
     * leftRight bits, there could be other threads modifying the _rivilr
     * variable when calling arrive() or depart(), so we need to use
     * atomic_fetch_add() instead of atomic_store(). We could also
     * use atomic_compare_exchange_strong() but then it would not be
     * starvation-free.
     *
     * Progress Condition: Blocking - (Starvation-Free on x86)
     */
    void toggleVersionAndWait(void) {
        long long lrivilr = _rivilr.load();
        // Toggle leftRight bit
        if (getLeftRight(lrivilr) == 0) {
            _rivilr.fetch_add(1LL << BIT_LEFTRIGHT);
        } else {
            _rivilr.fetch_add(-(1LL << BIT_LEFTRIGHT));
        }
        const int prevVersionIndex = getVersionIndex(lrivilr);
        const int nextVersionIndex = (prevVersionIndex+1) & 0x1;
        // Wait for Readers from next version
        while (!isEmpty(nextVersionIndex)) {
            std::this_thread::yield();
        }
        // Toggle the versionIndex bit
        if (getVersionIndex(lrivilr) == 0) {
            _rivilr.fetch_add(1LL << BIT_VERSIONINDEX);
        } else {
            _rivilr.fetch_add(-(1LL << BIT_VERSIONINDEX));
        }
        // Wait for Readers from previous version
        while (!isEmpty(prevVersionIndex)) {
            std::this_thread::yield();
        }
    }


    void writersLock() {
        _writersMutex.lock();
    }


    void writersUnlock() {
        _writersMutex.unlock();
    }


    // Should be called only from within a writersLock()/writersUnlock() block of code
    long currentLeftRight(void) { return getLeftRight(_rivilr.load()); }


private:
    // Conversion methods
    long long getReadIndicator0(long long lrivilr) const { return (lrivilr >> BIT_RI0) & MASK_COUNTER; }
    long long getReadIndicator1(long long lrivilr) const { return (lrivilr >> BIT_RI1) & MASK_COUNTER; }
    long long getLeftRight(long long lrivilr) const { return (lrivilr >> BIT_LEFTRIGHT) & 0x1; }
    long long getVersionIndex(long long lrivilr) const { return (lrivilr >> BIT_VERSIONINDEX) & 0x1; }

    long long composeRILRVI(long long ri0, long long ri1, long long localVI, long long localLR) {
        return ((ri0 << BIT_RI0) | (ri1 << BIT_RI1) | (localLR << BIT_LEFTRIGHT) | (localVI << BIT_VERSIONINDEX));
    }

    bool isEmpty(long long localVersionIndex) {
        const long long lrivilr = _rivilr.load();
        if (localVersionIndex == 0) {
            return getReadIndicator0(lrivilr) == 0;
        } else {
            return getReadIndicator1(lrivilr) == 0;
        }
    }
};
}

#endif /* _LEFT_RIGHT_ATOMIC_LONG_H_ */
