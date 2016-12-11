/******************************************************************************
 * Copyright (c) 2016, Pedro Ramalhete, Andreia Correia
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
#ifndef _STRESS_TEST_URCU_H_
#define _STRESS_TEST_URCU_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

#include "URCUGraceVersion.hpp"
#include "URCUTwoPhase.hpp"
#ifdef URCU_BULLET_PROOF_LIB
#include "urcu-bp.h"
#endif


using namespace std;
using namespace chrono;


/**
 * This is a stress test to cover the algorithms shown in the grace sharing paper
 *
 */
class StressTestURCU {

private:
    struct UserData  {
        long long seq;
        int tid;
        UserData(long long lseq, int ltid) {
            this->seq = lseq;
            this->tid = ltid;
        }
        UserData() {
            this->seq = -2;
            this->tid = -2;
        }
        UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

        bool operator < (const UserData& other) const {
            return seq < other.seq;
        }
        bool operator == (const UserData& other) const {
            return seq == other.seq && tid == other.tid;
        }
    };

    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };


    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

    URCUGraceVersion urcurv = {};
    URCUTwoPhase urcutp = {};

    enum URCUTestCase { ReadersVersion, TwoPhase, BulletProof };
    std::string TestCaseStr[3] = { "URCUReadersVersion", "URCUTwoPhase", "URCUBulletProof" };

public:
    StressTestURCU(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    void stress(URCUTestCase tc, const int updateRatio, const seconds testLengthSeconds) {
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };

        atomic<UserData*> udarray[100];
        for (int i = 0; i < 100; i++) udarray[i].store(new UserData(i, 0));

        std::cout << TestCaseStr[tc] << ":\n";

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&updateRatio,&quit,&startFlag,&tc,&udarray](int tid) {
            long long numOps = 0;
            long long sum = 0;
            while (!startFlag.load()) this_thread::yield();
            while (!quit.load()) {
                uint64_t seed = 1234567890L;
                for (int i = 0; i < 100; i++) { // 100 %
                    UserData *myud = new UserData(tid, i);
                    seed = randomLong(seed);
                    auto ix = (unsigned int)(seed%100);
                    if (ix < updateRatio) {
                        // I'm a Writer
                        UserData *tmp = udarray[ix].load();
                        while (!udarray[ix].compare_exchange_strong(tmp, myud)) {
                            tmp = udarray[ix].load();
                        }
                        switch(tc) {
                        case ReadersVersion:
                            urcurv.synchronize_rcu();
                            break;
                        case TwoPhase:
                            urcutp.synchronize_rcu();
                            break;
                        case BulletProof:
#ifdef URCU_BULLET_PROOF_LIB
                            ::synchronize_rcu();
#endif
                            break;
                        }
                        delete tmp;
                    } else {
                        // I'm a Reader
                        switch(tc) {
                        case ReadersVersion: {
                            urcurv.read_lock(tid);
                            sum += udarray[ix].load()->tid;
                            urcurv.read_unlock(tid);
                            break;
                        }
                        case TwoPhase: {
                            int cookie = urcutp.read_lock();
                            sum += udarray[ix].load()->tid;
                            urcutp.read_unlock(cookie);
                            break;
                        }
                        case BulletProof: {
#ifdef URCU_BULLET_PROOF_LIB
                            ::rcu_read_lock();
                            sum += udarray[ix].load()->tid;
                            ::rcu_read_unlock();
#endif
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
        };

        //if (irun == 0) std::cout << "##### " << urcu->className() << " #####  \n";
        thread rwThreads[numThreads];
        for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, tid);
        startFlag.store(true);
        // Sleep for 100 seconds
        this_thread::sleep_for(testLengthSeconds);
        quit.store(true);
        for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
        quit.store(false);
        startFlag.store(false);

        for (int i = 0; i < 100; i++) delete udarray[i].load();
    }


    /**
     * An imprecise but fast random number generator
     */
    uint64_t randomLong(uint64_t x) {
        x ^= x >> 12; // a
        x ^= x << 25; // b
        x ^= x >> 27; // c
        return x * 2685821657736338717LL;
    }



public:

    static void allTests() {
        vector<int> threadList = { 4, 8, 2, 3 };
        vector<int> ratioList = { 1, 10, 50, 90, 98, 100, 0 }; // Percentage ratio
        const seconds testLength = 10s;

        for (int iratio = 0; iratio < ratioList.size(); iratio++) {
            auto ratio = ratioList[iratio];
            for (int ithread = 0; ithread < threadList.size(); ithread++) {
                auto nThreads = threadList[ithread];
                StressTestURCU st(nThreads);
                std::cout << "\n----- URCU Stress Test   ratio=" << ratio << "%   numThreads=" << nThreads << "   length=" << testLength.count() << "s -----\n";
                st.stress(ReadersVersion, ratio, testLength);
                st.stress(TwoPhase, ratio, testLength);
// To run with BP, comment out the synchronize_rcu() calls of the others, and benchmark() and run with
// LD_LIBRARY_PATH=. ./urcu-bp
#ifdef URCU_BULLET_PROOF_LIB
                st.stress(BulletProof, ratio, testLength, numRuns);
#endif
            }
        }
    }
};

#endif
