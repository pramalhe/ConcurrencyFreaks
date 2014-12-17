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

#ifndef _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_H_
#define _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_H_

#include <atomic>
#include <mutex>
#include <thread>

namespace LeftRight {

static const long READS_ON_LEFT  = 0;
static const long READS_ON_RIGHT = 1;


/**
 * Left-Right Atomic Long No Version variant
 * Uses a single atomic long for all counters and leftRight variable and does
 * not need a versionIndex.
 *
 * arrive()               - Wait-Free (on x86)
 * depart()               - Wait-Free (on x86)
 * toggleVersionAndWait() - Blocking
 * writersLock()          - Blocking
 * witersUnlock()         - Wait-Free
 *
 * We used the Left-Right pattern described in:
 http://concurrencyfreaks.com/2013/12/left-right-concurrency-control.html
 * with the Atomic Long No Version variant implementation described in:
 * http:...
 * <p>
 * TODO: Do some unit testing on the overflow mechanism in depart()
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LeftRightALNV {

private:
    // Position of contents of leftRightCounters
    static const int BIT_INGRESS        = 0;
    static const int BIT_EGRESS_LEFT    = (20);
    static const int BIT_EGRESS_RIGHT   = (20+20);
    static const int BIT_LEFTRIGHT      = (20+20+20);

    // Bitmask of a counter (20 bits)
    static const long long MASK_COUNTER = (1LL << 20) - 1LL;

    // Value of counter after which an overflow handling is triggered
    static const long long OVERFLOW_COUNTER = MASK_COUNTER << 1;

    // Members used by the Left-Right mechanism
    std::atomic<long long>  _leftRightCounters;
    std::mutex              _writersMutex;

public:
    // Yes, we want these public
    //T _instanceLeft;
    //T _instanceRight;

    LeftRightALNV() {
        _leftRightCounters.store(composeLRC(READS_ON_LEFT, 0, 0, 0));
    }

    /*
    LeftRightALNV(T linst, T rinst) {
        _instanceLeft = linst;
        _instanceRight = rinst;
        _leftRightCounters.store(composeLRC(READS_ON_LEFT, 0, 0, 0));
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
        const long long lrc = _leftRightCounters.fetch_add(1LL << BIT_INGRESS);
        return getLeftRight(lrc);
    }


    /**
     * Marks that a Reader has departed from the readIndicator.
     *
     * Don't be deceived by the apparently infinite while() loop. This
     * loop will run at most the number of threads doing arrive()/depart()
     * and as such, it is wait-free, but not wfpo.
     *
     * Progress Condition: Wait-Free (on x86)
     *
     * @param localVersionIndex
     */
    void depart(int localLeftRight) {
        long long lrc;
        if (localLeftRight == READS_ON_LEFT) {
            lrc = _leftRightCounters.fetch_add(1LL << BIT_EGRESS_LEFT);
        } else {
            lrc = _leftRightCounters.fetch_add(1LL << BIT_EGRESS_RIGHT);
        }
        // Check if we need to handle a possible overflow
        while (true) {
            const long long ingress = getIngress(lrc);
            if (ingress < OVERFLOW_COUNTER) return;
            if (getLeftRight(lrc) == READS_ON_LEFT) {
                const long long egress = getEgressLeft(lrc);
                if (_leftRightCounters.compare_exchange_strong(lrc, composeLRC(READS_ON_LEFT, 0, egress-ingress, 0))) return;
            } else {
                const long long egress = getEgressRight(lrc);
                if (_leftRightCounters.compare_exchange_strong(lrc, composeLRC(READS_ON_RIGHT, egress-ingress, 0, 0))) return;
            }
            lrc = _leftRightCounters.load();
        }
    }


    /**
     * Waits for all the threads doing a "Read" to finish their tasks on the
     * Set that the "Writer" wants to modify.
     * Must be called only by "Writer" operations, and {@code writersMutex}
     * must be locked when this function is called.
     *
     * Progress Condition: Blocking
     */
    void toggleVersionAndWait(void) {
        const long long oldlrc = _leftRightCounters.load();
        if (getLeftRight(oldlrc) == READS_ON_LEFT) {
            // Toggle versionIndex, and reset all counters
            const long long lastlrc = _leftRightCounters.exchange(composeLRC(READS_ON_RIGHT, 0, 0, 0));
            const long long lastIngress = getIngress(lastlrc);
            const long long lastEgressLeft = getEgressLeft(lastlrc);
            // Wait for EgressLeft to match the last seen ingress before toggle
            if (lastEgressLeft != lastIngress) {
                while (!isEmpty(READS_ON_LEFT, lastEgressLeft, lastIngress)) std::this_thread::yield();
            }
        } else {
            // leftRight is READS_ON_RIGHT
            const long long lastlrc = _leftRightCounters.exchange(composeLRC(READS_ON_LEFT, 0, 0, 0));
            const long long lastIngress = getIngress(lastlrc);
            const long long lastEgressRight = getEgressRight(lastlrc);
            if (lastEgressRight != lastIngress) {
                while (!isEmpty(READS_ON_RIGHT, lastEgressRight, lastIngress)) std::this_thread::yield();
            }
        }
    }


    void writersLock() {
        _writersMutex.lock();
    }


    void writersUnlock() {
        _writersMutex.unlock();
    }


    // Should be called only from within a writersLock()/writersUnlock() block of code
    long currentLeftRight(void) { return getLeftRight(_leftRightCounters.load()); }


private:
    // Conversion methods
    long long getIngress(long long lrc) const { return (lrc >> BIT_INGRESS) & MASK_COUNTER; }
    long long getEgressLeft(long long lrc) const { return (lrc >> BIT_EGRESS_LEFT) & MASK_COUNTER; }
    long long getEgressRight(long long lrc) const { return (lrc >> BIT_EGRESS_RIGHT) & MASK_COUNTER; }
    long long getLeftRight(long long lrc) const { return (lrc >> BIT_LEFTRIGHT) & 0x1; }

    long long composeLRC(long long leftRight, long long egressRight, long long egressLeft, long long ingress) {
        return ((leftRight << BIT_LEFTRIGHT) | (egressLeft << BIT_EGRESS_RIGHT) |
                (egressRight << BIT_EGRESS_LEFT) | (ingress << BIT_INGRESS));
    }

    bool isEmpty(long long localLeftRight, long long localEgressAdd, long long lastIngress) {
        const long long lrc = _leftRightCounters.load();
        const long long egress = (localLeftRight == READS_ON_LEFT) ? getEgressLeft(lrc) : getEgressRight(lrc);
        return ((egress + localEgressAdd) == lastIngress);
    }
};
}

#endif /* _LEFT_RIGHT_ATOMIC_LONG_NO_VERSION_H_ */
