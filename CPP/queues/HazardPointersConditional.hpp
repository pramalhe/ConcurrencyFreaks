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

#ifndef _HAZARD_POINTERS_COND_H_
#define _HAZARD_POINTERS_COND_H_

#include <atomic>
#include <iostream>


template<typename T>
class HazardPointersConditional {

private:
    static const int      HP_MAX_THREADS = 128;
    static const int      HP_MAX_HPS = 4;     // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
    static const int      MAX_RETIRED = HP_MAX_THREADS*HP_MAX_HPS; // Maximum number of retired objects per thread

    const int             maxHPs;
    const int             maxThreads;
    std::atomic<T*>       hp[HP_MAX_THREADS*CLPAD][HP_MAX_HPS];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];

public:
    HazardPointersConditional(int maxHPs=HP_MAX_HPS, int maxThreads=HP_MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            for (int ihp = 0; ihp < HP_MAX_HPS; ihp++) {
                hp[ithread*CLPAD][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~HazardPointersConditional() {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[ithread*CLPAD].size(); iret++) {
                delete retiredList[ithread*CLPAD][iret];
            }
        }
    }

    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid*CLPAD][ihp].store(nullptr, std::memory_order_release);
        }
    }

    T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
		while ((ret = atom.load()) != n) {
			hp[tid*CLPAD][index].store(ret);
			n = ret;
		}
		return ret;
    }

    // This returns the same value that is passed as ptr, which is sometimes usefull
    T* protectPtr(int index, T* ptr, const int tid) {
        hp[tid*CLPAD][index].store(ptr);
        return ptr;
    }

    void retire(T* ptr, const int tid) {
        retiredList[tid*CLPAD].push_back(ptr);
        if (retiredList[tid*CLPAD].size() < HP_THRESHOLD_R) return;
        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            auto obj = retiredList[tid*CLPAD][iret];
            if (obj->item.load() != nullptr) {
                iret++;
                continue;  // Delete only if Node.item == nullptr
            }
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads && canDelete; tid++) {
                for (int ihp = maxHPs-1; ihp >= 0; ihp--) {
                    if (hp[tid*CLPAD][ihp].load() == obj) {
                        canDelete = false;
                        break;
                    }
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

#endif /* _HAZARD_POINTERS_CONDITIONAL_H_ */
