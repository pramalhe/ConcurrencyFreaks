/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _HAZARD_POINTERS_DL_H_
#define _HAZARD_POINTERS_DL_H_

#include <atomic>
#include <iostream>


template<typename T>
class HazardPointersDL {

private:
    static const int      HP_MAX_THREADS = 128;
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
    static const int      MAX_RETIRED = HP_MAX_THREADS; // Maximum number of retired objects per thread

    const int             maxThreads;

    std::atomic<T*>       hp[HP_MAX_THREADS*CLPAD];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];

public:
    HazardPointersDL(int maxThreads=HP_MAX_THREADS) : maxThreads{maxThreads} {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            hp[ithread*CLPAD].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~HazardPointersDL() {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[ithread*CLPAD].size(); iret++) {
                delete retiredList[ithread*CLPAD][iret];
            }
        }
    }


    /**
     * Progress Condition: wait-free
     */
    void clear(const int tid) {
        hp[tid*CLPAD].store(nullptr, std::memory_order_release);
    }

	
    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    T* protectPtr(T* ptr, const int tid) {
        hp[tid*CLPAD].store(ptr);
        return ptr;
    }


    /**
     * Progress Condition: wait-free bounded (by the number of threads squared)
     *
     * We can not delete a node if:
     * - There is an hazard pointer for it;
     * - There is an hazard pointer for its "next": It might dereference node->prev in enqueue()
     * - There is an hazard pointer for its "prev": It might dereference node->next in dequeue()
     */
    void retire(T* ptr, T* ltail, const int tid) {
        retiredList[tid*CLPAD].push_back(ptr);
        if (retiredList[tid*CLPAD].size() < HP_THRESHOLD_R) return;
        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            auto obj = retiredList[tid*CLPAD][iret];
            if (obj->next.load() == ltail) {
                iret++;
                continue; // Can't delete the node previous to the current tail
            }
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads; tid++) {
                T* curhp = hp[tid*CLPAD].load();
                if (obj == curhp || obj->next.load() == curhp || obj->prev == curhp) {
                    canDelete = false;
                    break;
                }
            }
            if (canDelete) {
                retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }
};

#endif /* _HAZARD_POINTERS_DL_H_ */
