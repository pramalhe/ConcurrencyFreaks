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

#ifndef _READINDICATOR_ATOMIC_COUNTER_H_
#define _READINDICATOR_ATOMIC_COUNTER_H_

#include <atomic>


/**
 * <h1> Atomic Counter ReadIndicator </h1>
 * Use a single atomic long to act as a ReadIndicator
 * <p>
 * Progress Conditions: <ul>
 * <li>arrive()  - O(1), Wait-Free Population Oblivious (on x86)
 * <li>depart()  - O(1), Wait-Free Population Oblivious (on x86)
 * <li>isEmpty() - O(1), Wait-Free Population Oblivious (on x86)
 * </ul>
 * Advantages: <ul>
 * <li> Low memory footprint
 * <li> WFPO progress conditions on x86
 * </ul>
 * <p>
 * Disadvantages: <ul>
 * <li> Doesn't scale well under a high number of Readers
 * </ul>
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class RIAtomicCounter {

private:
    std::atomic<uint64_t> counter { 0 };

public:
    RIAtomicCounter() { }

    void arrive(void) {
        counter.fetch_add(1);
    }

    void depart(void) {
        counter.fetch_add(-1);
    }

    bool isEmpty(void) {
        return counter.load() == 0;
    }
};

#endif /* _READINDICATOR_ATOMIC_COUNTER_H_ */
