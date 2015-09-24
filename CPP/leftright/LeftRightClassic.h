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

#ifndef _LEFT_RIGHT_CLASSIC_H_
#define _LEFT_RIGHT_CLASSIC_H_

#include <atomic>
#include <mutex>
#include <thread>
#include "ReadIndicator.h"
#include "RIAtomicCounter.h"

namespace LeftRight {

/**
 * Left-Right - Classic variant
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
template<class RI = RIAtomicCounter>
class LeftRightClassic {

private:
    // Members used by the Left-Right mechanism
    RI              *_readersVersion[2];
    std::atomic<int> _versionIndex __attribute__(( aligned(64) )) { 0 } ;

public:
    std::mutex       _writersMutex __attribute__(( aligned(64) ));

    LeftRightClassic() {
        _readersVersion[0] = new RI();
        _readersVersion[1] = new RI();
    }

    ~LeftRightClassic() {
        delete _readersVersion[0];
        delete _readersVersion[1];
    }


    /**
     * Marks that a new Reader has arrived at the readIndicator.
     *
     * Progress Condition: Progress of ReadIndicator.arrive()
     *
     * @return the current versionIndex
     */
    const int arrive(void) {
        const int localVI = _versionIndex.load();
        _readersVersion[localVI]->arrive();
        return localVI;
    }


    /**
     * Marks that a Reader has departed from the readIndicator.
     *
     * Progress Condition: Progress of ReadIndicator.depart()
     *
     * @param localVI Pass the value returned by arrive()
     */
    void depart(const int localVI) {
        _readersVersion[localVI]->depart();
    }

    /**
     * Waits for all the Arrivers that called arrive() before the _versionIndex
     * changed, to call depart().
     * Must be called only by a Toggler, and {@code writersMutex}
     * must be locked when this function is called.
     *
     * Progress Condition: Blocking
     *
     * @return
     */
    void toggleVersionAndWait(void) {
        const int localVI = _versionIndex.load();
        const int prevVI = localVI & 0x1;
        const int nextVI = (localVI+1) & 0x1;

        // Wait for Readers from next version
        while (!_readersVersion[nextVI]->isEmpty()) {
            std::this_thread::yield();
        }

        // Toggle the versionIndex variable
        _versionIndex.store(nextVI);

        // Wait for Readers from previous version
        while (!_readersVersion[prevVI]->isEmpty()) {
            std::this_thread::yield();
        }
    }


    void writersLock() {
        _writersMutex.lock();
    }


    void writersUnlock() {
        _writersMutex.unlock();
    }
};
}

#endif /* _LEFT_RIGHT_CLASSIC_H_ */
