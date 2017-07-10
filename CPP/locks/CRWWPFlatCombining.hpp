/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _CRWWP_FLAT_COMBINING_H_
#define _CRWWP_FLAT_COMBINING_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include "RIStaticPerThread.hpp"

/**
 * <h1> C-RW-WP with Flat Combining </h1>
 *
 * This is a blocking universal construct that protects the object or data
 * structure with a single C-RW-WP using Flat Combining
 * Cohort lock is a spin lock because Flat Combining gives starvation freedom to writers.
 * The Writers may help readers which means that Writers do _not_ starve readers,
 * Unlike on the original C-RW-WP, this technique is fully starvation-free (writers to readers,
 * readers to writers, writers to writers, and readers to readers).
 *
 * If having an array with unique entries for tid is too difficult to achieve, then
 * use the same trick as in Hazard Pointers, with a linked list of "re-usable" nodes
 * instead of entries in an array.
 *
 * C-RW-WP paper:         http://dl.acm.org/citation.cfm?id=2442532
 * Flat Combining paper:  http://dl.acm.org/citation.cfm?id=1810540
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename C, typename R = bool>
class CRWWPFlatCombining {

private:
    static const int CLPAD = 128/sizeof(uintptr_t);
    static const int MAX_THREADS = 128;
    static const int LOCKED = 1;
    static const int UNLOCKED = 0;
    const int maxThreads;
    RIStaticPerThread ri { MAX_THREADS };
    alignas(128) std::atomic<int> cohort { UNLOCKED };
    alignas(128) C* instance;
    alignas(128) std::atomic< std::function<R(C*)>* >* fc;
    alignas(128) R* results;

public:
    CRWWPFlatCombining(C* inst, const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        instance = inst;
        fc = new std::atomic< std::function<R(C*)>* >[maxThreads*CLPAD];
        results = new R[maxThreads*CLPAD];
        for (int i = 0; i < maxThreads; i++) {
            fc[i*CLPAD].store(nullptr, std::memory_order_relaxed);
        }
    }


    ~CRWWPFlatCombining() {
        delete instance;
        delete[] fc;
        delete[] results;
    }


    R applyMutation(std::function<R(C*)>& mutativeFunc, const int tid) {
        // Add our mutation to the array of flat combining
        fc[tid*CLPAD].store(&mutativeFunc, std::memory_order_release);

        // lock()
        while (true) {
            int unlocked = UNLOCKED;
            if (cohort.load() == UNLOCKED &&
                cohort.compare_exchange_strong(unlocked, LOCKED)) break;
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                return results[tid*CLPAD];
            }
            std::this_thread::yield(); // pause()
        }
        while (!ri.isEmpty()) {
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                cohort.store(UNLOCKED, std::memory_order_release);
                return results[tid*CLPAD];
            }
            std::this_thread::yield(); // pause()
        }

        // For each mutation in the flat combining array, apply it in the order
        // of the array, save the result, and set the entry in the array to nullptr
        for (int i = 0; i < maxThreads; i++) {
            auto mutation = fc[i*CLPAD].load(std::memory_order_acquire);
            if (mutation == nullptr) continue;
            results[i*CLPAD] = (*mutation)(instance);
            fc[i*CLPAD].store(nullptr, std::memory_order_release);
        }

        // unlock()
        cohort.store(UNLOCKED, std::memory_order_release);
        return results[tid*CLPAD];
    }


    R applyRead(std::function<R(C*)>& readFunc, const int tid) {
        bool announced = false;
        // lock()
        while (true) {
            ri.arrive(tid);
            if (cohort.load() == UNLOCKED) break;
            ri.depart(tid);
            if (!announced) {
                // Put my operation in the flat combining array for a Writer to do it
                fc[tid*CLPAD].store(&readFunc, std::memory_order_release);
                announced = true;
            }
            // If a Writer set our entry to nullptr then the result is ready
            while (cohort.load() == LOCKED) {
                if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                    return results[tid*CLPAD];
                }
                std::this_thread::yield();  // pause()
            }
        }

        R result = readFunc(instance);
        if (announced) fc[tid*CLPAD].store(nullptr, std::memory_order_relaxed);

        // unlock()
        ri.depart(tid);
        return result;
    }

};


// This class can be used to simplify the usage of sets/multisets with CXMutation.
// For generic code you don't need it (and can't use it), but it can serve as an example of how to use lambdas.
// C must be a set/multiset where the keys are of type CKey
template<typename C, typename CKey>
class CRWWPFlatCombiningSet {
private:
    static const int MAX_THREADS = 128;
    const int maxThreads;
    CRWWPFlatCombining<C> crwwp{new C(), maxThreads};

public:
    CRWWPFlatCombiningSet(const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} { }

    static std::string className() { return "CRWWPFlatCombining-" + C::className(); }

    // Progress-condition: blocking
    bool add(CKey* key, const int tid) {
        std::function<bool(C*)> addFunc = [key] (C* set) { return set->add(key); };
        return crwwp.applyMutation(addFunc, tid);
    }

    // Progress-condition: blocking
    bool remove(CKey* key, const int tid) {
        std::function<bool(C*)> removeFunc = [key] (C* set) { return set->remove(key); };
        return crwwp.applyMutation(removeFunc, tid);
    }

    // Progress-condition: blocking
    bool contains(CKey* key, const int tid) {
        std::function<bool(C*)> containsFunc = [key] (C* set) { return set->contains(key); };
        return crwwp.applyRead(containsFunc, tid);
    }

    // Progress-condition: blocking
    void addAll(CKey** keys, const int size, const int tid) {
        std::function<bool(C*)> addFunc = [keys,size] (C* set) {
            for (int i = 0; i < size; i++) set->add(keys[i]);
            return true;
        };
        crwwp.applyMutation(addFunc, tid);
    }
};

#endif /* _CRWWP_UNIVERSAL_H_ */
