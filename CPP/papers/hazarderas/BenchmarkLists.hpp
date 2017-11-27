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
#ifndef _BENCHMARK_LISTS_H_
#define _BENCHMARK_LISTS_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include "MagedHarrisLinkedListNone.hpp"
#include "MagedHarrisLinkedListHP.hpp"
#include "MagedHarrisLinkedListHE.hpp"
#include "MagedHarrisLinkedListURCU.hpp"
//#include "MagedHarrisLinkedListHPLB.hpp"
//#include "MagedHarrisLinkedListHPLB2.hpp"
//#include "MagedHarrisLinkedListHERange.hpp"
//#include "MagedHarrisLinkedListHEWF.hpp"

using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark to run the tests shown in Hazard Eras paper
 */
class BenchmarkLists {

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

public:
    BenchmarkLists(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    template<typename L>
    long long benchmark(const int updateRatio, const seconds testLengthSeconds, const int numRuns, const int numElements) {
        long long ops[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        L* list = nullptr;

        // Create all the objects in the list
        UserData* udarray[numElements];
        for (int i = 0; i < numElements; i++) udarray[i] = new UserData(i, 0);

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&updateRatio,&quit,&startFlag,&list,&udarray,&numElements](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t seed = tid+1234567890123456781ULL;
            while (!startFlag.load()) { } // spin
            while (!quit.load()) {
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%numElements);
                seed = randomLong(seed);
                auto ratio = seed%10000;  // Ratios are in per-10k units
                if (ratio < updateRatio) {
                    // I'm a Writer
                    if (list->remove(udarray[ix], tid)) list->add(udarray[ix], tid);
                } else {
                    // I'm a Reader
                    list->contains(udarray[ix], tid);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%numElements);
                    list->contains(udarray[ix], tid);
                }
                numOps+=2;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            list = new L(numThreads);
            // Add all the items to the list
            for (int i = 0; i < numElements; i++) list->add(udarray[i], 0);
            if (irun == 0) cout << "##### " << list->className() << " #####  \n";
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            startFlag.store(true);
            // Sleep for 100 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            quit.store(false);
            startFlag.store(false);
            delete list;
        }

        for (int i = 0; i < numElements; i++) delete udarray[i];

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median, max and min. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
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

    static void allThroughputTests() {
        //vector<int> threadList = { 1, 2, 4, 8, 16, 20, 24, 28, 32, 34, 36, 48, 64 }; // Number of threads for Opteron
        vector<int> threadList = { 1, 2, 4 };         // Number of threads for the laptop
        vector<int> ratioList = { 0 /*10000, 1000, 100, 0*/ }; // per-10k ratio: 100%, 10%, 1%, 0%
        const int numRuns = 5;                           // 5 runs for the paper
        const seconds testLength = 10s;                  // 20s for the paper
        vector<int> elemsList = { /*100, 1000,*/ 10000 };    // Number of keys in the set: 100, 1k, 10k

        // Save results
        // [class][ratio][threads]
        const int LNO = 0;
        const int LHP = 1;
        const int LHE = 2;
        const int LUR = 3;
        const int LUD = 4;
        const int LLB = 5;
        const int LHR = 6;
        const int LWF = 7;
        long long ops[7][ratioList.size()][threadList.size()];

        for (unsigned ielem = 0; ielem < elemsList.size(); ielem++) {
            auto numElements = elemsList[ielem];
            for (int iratio = 0; iratio < ratioList.size(); iratio++) {
                auto ratio = ratioList[iratio];
                for (int ithread = 0; ithread < threadList.size(); ithread++) {
                    auto nThreads = threadList[ithread];
                    BenchmarkLists bench(nThreads);
                    std::cout << "\n----- Lists Benchmark   numElements=" << numElements << "   ratio=" << ratio/100 << "%   numThreads=" << nThreads << "   numRuns=" << numRuns << "   length=" << testLength.count() << "s -----\n";
                    ops[LNO][iratio][ithread] = 0;//bench.benchmark<MagedHarrisLinkedListNone<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LHP][iratio][ithread] = bench.benchmark<MagedHarrisLinkedListHP<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LHE][iratio][ithread] = bench.benchmark<MagedHarrisLinkedListHE<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LUR][iratio][ithread] = bench.benchmark<MagedHarrisLinkedListURCU<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LLB][iratio][ithread] = 0;//bench.benchmark<MagedHarrisLinkedListHPLB<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LUD][iratio][ithread] = 0;//bench.benchmark<MagedHarrisLinkedListHPLB2<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LHR][iratio][ithread] = 0;//bench.benchmark<MagedHarrisLinkedListHERange<UserData>>(ratio, testLength, numRuns, numElements);
                    ops[LWF][iratio][ithread] = 0;//bench.benchmark<MagedHarrisLinkedListHEWF<UserData>>(ratio, testLength, numRuns, numElements);
                }
            }
        }

        // Show results in .csv format
        cout << "\n\nResults in ops per second for numRuns=" << numRuns << ",  length=" << testLength.count() << "s \n";
        for (unsigned ielem = 0; ielem < elemsList.size(); ielem++) {
            auto numElements = elemsList[ielem];
            std::cout << "\nNumber of elements: " << numElements << "\n";
            for (int iratio = 0; iratio < ratioList.size(); iratio++) {
                auto ratio = ratioList[iratio];
                cout << "Ratio " << ratio/100. << "%\n";
                cout << "Threads, No Reclamation, Hazard Pointers, Hazard Eras, URCU, Hazard Pointers (Linear Bound), HP LB2, HE Range, HE WF\n";
                for (int ithread = 0; ithread < threadList.size(); ithread++) {
                    auto nThreads = threadList[ithread];
                    cout << nThreads << ", ";
                    for (int il = 0; il < 8; il++) {
                        cout << ops[il][iratio][ithread] << ", ";
                    }
                    cout << "\n";
                }
            }
        }
    }
};

#endif
