/******************************************************************************
 * Copyright (c) 2016-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _HAZARD_ERAS_H_
#define _HAZARD_ERAS_H_

#include <atomic>
#include <iostream>
#include <vector>
#include <algorithm>

/*
 * <h1> Hazard Eras </h1>
 * This a light-weight implementation of hazard eras, where each thread has a
 * thread-local list of retired objects.
 *
 * This is based on the paper "Hazard Eras - Non-Blocking Memory Reclamation"
 * by Pedro Ramalhete and Andreia Correia:
 * https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/hazarderas-2017.pdf
 *
 * The type T is for the objects/nodes and it's it must have the members newEra, delEra
 *
 * R is zero.
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class HazardEras {

private:
    static const uint64_t NONE = 0;
    static const int      HE_MAX_THREADS = 128;
    static const int      MAX_HES = 5;        // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HE_THRESHOLD_R = 0; // This is named 'R' in the HP paper

    const int             maxHEs;
    const int             maxThreads;

    alignas(128) std::atomic<uint64_t>  eraClock {1};
    alignas(128) std::atomic<uint64_t>* he[HE_MAX_THREADS];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    alignas(128) std::vector<T*>        retiredList[HE_MAX_THREADS*CLPAD];

public:
    HazardEras(int maxHEs=MAX_HES, int maxThreads=HE_MAX_THREADS) : maxHEs{maxHEs}, maxThreads{maxThreads} {
        for (int it = 0; it < HE_MAX_THREADS; it++) {
            he[it] = new std::atomic<uint64_t>[CLPAD*2]; // We allocate four cache lines to allow for many hps and without false sharing
            retiredList[it*CLPAD].reserve(maxThreads*maxHEs);
            for (int ihe = 0; ihe < MAX_HES; ihe++) {
                he[it][ihe].store(NONE, std::memory_order_relaxed);
            }
        }
        static_assert(std::is_same<decltype(T::newEra), uint64_t>::value, "T::newEra must be uint64_t");
        static_assert(std::is_same<decltype(T::delEra), uint64_t>::value, "T::delEra must be uint64_t");
    }

    ~HazardEras() {
        for (int it = 0; it < HE_MAX_THREADS; it++) {
            delete[] he[it];
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[it*CLPAD].size(); iret++) {
                delete retiredList[it*CLPAD][iret];
            }
        }
    }


    inline uint64_t getEra() {
        return eraClock.load();
    }


    /**
     * Progress Condition: wait-free bounded (by maxHEs)
     */
    inline void clear(const int tid) {
        for (int ihe = 0; ihe < maxHEs; ihe++) {
            he[tid][ihe].store(NONE, std::memory_order_release);
        }
    }


    /**
     * Progress Condition: lock-free
     */
    inline T* get_protected(int index, const std::atomic<T*>& atom, const int tid) {
        auto prevEra = he[tid][index].load(std::memory_order_relaxed);
		while (true) {
		    T* ptr = atom.load();
		    auto era = eraClock.load(std::memory_order_acquire);
		    if (era == prevEra) return ptr;
            he[tid][index].store(era);
            prevEra = era;
		}
    }

    inline void protectEraRelease(int index, int other, const int tid) {
        auto era = he[tid][other].load(std::memory_order_relaxed);
        if (he[tid][index].load(std::memory_order_relaxed) == era) return;
        he[tid][index].store(era, std::memory_order_release);
    }


    /*
     * Does a single iteration. Must be integrated into the algorithm that's using HE.
     * In other words, we must re-check if era has changed
     *
     * Progress Condition: wait-free population oblivious
     */
    inline T* protectPtr(int index, const std::atomic<T*>& atom, uint64_t& prevEra, const int tid) {
        T* ptr = atom.load(std::memory_order_acquire);
        auto era = eraClock.load();
        if (prevEra != era) {
            prevEra = era;
            he[tid][index].store(era, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
        return ptr;
    }


    /**
     * Retire an object (node)
     * Progress Condition: wait-free bounded
     *
     * Doing rlist.erase() is not the most efficient way to remove entries from a std::vector, but ok...
     */
    void retire(T* ptr, const int mytid) {
        auto currEra = eraClock.load();
        ptr->delEra = currEra;
        auto& rlist = retiredList[mytid*CLPAD];
        rlist.push_back(ptr);
        if (eraClock == currEra) eraClock.fetch_add(1);
        for (unsigned iret = 0; iret < rlist.size();) {
            auto obj = rlist[iret];
            if (canDelete(obj, mytid)) {
                rlist.erase(rlist.begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }

private:
    bool canDelete(T* obj, const int mytid) {
        for (int tid = 0; tid < maxThreads; tid++) {
            for (int ihe = 0; ihe < maxHEs; ihe++) {
                const auto era = he[tid][ihe].load(std::memory_order_acquire);
                if (era == NONE || era < obj->newEra || era > obj->delEra) continue;
                return false;
            }
        }
        return true;
    }

};

#endif /* _HAZARD_ERAS_H_ */
