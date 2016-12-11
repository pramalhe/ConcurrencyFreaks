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
#ifndef _BENCHMARK_URCU_H_
#define _BENCHMARK_URCU_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

#include "RIAtomicCounter.hpp"
#include "RIAtomicCounterArray.hpp"
#include "RIEntryPerThread.hpp"
#include "URCUTwoPhase.hpp"
#include "URCUGraceVersion.hpp"
#include "URCUGraceVersionSyncScale.hpp"
#ifdef URCU_BULLET_PROOF_LIB
#include "urcu-bp.h"
#endif
#ifdef URCU_MB
#include "urcu.h"
#endif
#ifdef LINUX_URCU
#include "urcu.h"
#endif


using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark to run the tests shown in the Grace Sharing paper
 *
 * <h2> Throughput Benchmarks </h2>
 * - 0% modifications
 * - 1% modifications
 * - 10% modifications
 * - 50% modifications
 * - 100% modifications
 * ... for x86 and ARMv7 ?
 *
 */
class BenchmarkURCU {

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

    const int numThreads;

    URCUGraceVersion urcurv {numThreads};
    URCUGraceVersionSyncScale urcurvss {};
    URCUTwoPhase<RIEntryPerThread> urcu_tpept {};
    URCUTwoPhase<RIAtomicCounterArray> urcu_tpaca {};


    enum URCUTestCase { GraceVersion, GraceVersionSyncScale, TwoPhaseEntryPerThread, TwoPhaseAtomicCounterArray, BulletProof };
    std::string TestCaseStr[5] = { "URCUGraceVersion", "URCUGraceVersionSyncScale", "TwoPhase-EntryPerThread", "TwoPhase-AtomicCounterArray", "URCUBulletProof" };

public:
    BenchmarkURCU(const int numThreads) : numThreads{numThreads} {
    }


    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    long long benchmark(URCUTestCase tc, const int updateRatio, const seconds testLengthSeconds, const int numRuns) {
        long long ops[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };

        std::cout << TestCaseStr[tc] << ":\n";

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&updateRatio,&quit,&startFlag,&tc](int tid, long long *ops) {
            long long numOps = 0;
            long long sum = 0;
#ifdef LINUX_URCU
            rcu_register_thread();
#endif
            while (!startFlag.load()) this_thread::yield();
            while (!quit.load()) {
                uint64_t seed = 1234567890L;
                for (int i = 0; i < 100; i++) { // 100 %
                    seed = randomLong(seed);
                    auto ix = (unsigned int)(seed%100);
#if defined(URCU_BULLET_PROOF_LIB) || defined(LINUX_URCU)
                    if (ix < updateRatio) {
                        // I'm a Writer
                        switch(tc) {
                        case BulletProof:
                            ::synchronize_rcu();
                            break;
                        default:
                            break;
                        }
                    } else {
                        // I'm a Reader
                        switch(tc) {
                        case BulletProof: {
                            ::rcu_read_lock();
                            quit.load();
                            ::rcu_read_unlock();
                            break;
                        }
                        default:
                            break;
                        }
                    }
#else
                    if (ix < updateRatio) {
                        // I'm a Writer
                        switch(tc) {
                        case GraceVersion:
                            urcurv.synchronize_rcu();
                            break;
                        case GraceVersionSyncScale:
                            urcurvss.synchronize_rcu(tid);
                            break;
                        case TwoPhaseEntryPerThread:
                            urcu_tpept.synchronize_rcu();
                            break;
                        case TwoPhaseAtomicCounterArray:
                            urcu_tpaca.synchronize_rcu();
                            break;
                        case BulletProof:
                            //::synchronize_rcu();
                            break;
                        }
                    } else {
                        // I'm a Reader
                        switch(tc) {
                        case GraceVersion: {
                            urcurv.read_lock(tid);
                            quit.load();
                            urcurv.read_unlock(tid);
                            break;
                        }
                        case GraceVersionSyncScale: {
                            urcurvss.rcu_read_lock(tid);
                            quit.load();
                            urcurvss.rcu_read_unlock(tid);
                            break;
                        }
                        case TwoPhaseEntryPerThread: {
                            int cookie = urcu_tpept.rcu_read_lock(tid);
                            quit.load();
                            urcu_tpept.rcu_read_unlock(cookie, tid);
                            break;
                        }
                        case TwoPhaseAtomicCounterArray: {
                            int cookie = urcu_tpaca.rcu_read_lock();
                            quit.load();
                            urcu_tpaca.rcu_read_unlock(cookie);
                            break;
                        }
                        default:
                            break;
                        }
                    }
#endif
                }
                numOps += 100;
            }
            *ops = numOps;
#ifdef LINUX_URCU
            rcu_unregister_thread();
#endif
        };

        for (int irun = 0; irun < numRuns; irun++) {
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, tid, &ops[tid][irun]);
            startFlag.store(true);
            // Sleep for 20 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            quit.store(false);
            startFlag.store(false);
        }

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        long long result = agg[numRuns/2]/testLengthSeconds.count();
        cout << "Ops/sec=" << result << "\n";
        return result;
    }


    /**
     * In this benchmark there are two threads that are continously reading and the others
     * are only calling synchronize_rcu. The length of the reading time is somewhat long
     */
    long long benchmark2Readers(URCUTestCase tc, const seconds testLengthSeconds, const int numRuns) {
        long long ops[numThreads][numRuns];
        long long opsReaders[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        const int readLength = 100000; // 100000 is a long read-side critical section
        atomic<long> readvars[readLength];

        for (int i = 0; i < readLength; i++) readvars[i].store(i, std::memory_order_relaxed);

        std::cout << TestCaseStr[tc] << ":\n";

        auto reader_lambda = [this,&quit,&startFlag,&tc,&readvars](int tid, long long *opsReaders) {
            long long sum = 0;
            long long numOps = 0;
#ifdef LINUX_URCU
            rcu_register_thread();
#endif
            while (!startFlag.load()) this_thread::yield();
            while (!quit.load()) {
#if defined(URCU_BULLET_PROOF_LIB) || defined(LINUX_URCU)
                switch(tc) {
                case BulletProof: {
                    ::rcu_read_lock();
                    for (int i = 0; i < readLength; i++) sum += readvars[i].load();
                    ::rcu_read_unlock();
                    break;
                }
                default:
                    break;
                }
#else
                switch(tc) {
                case GraceVersion: {
                    urcurv.read_lock(tid);
                    for (int i = 0; i < readLength; i++) sum += readvars[i].load();
                    urcurv.read_unlock(tid);
                    break;
                }
                case GraceVersionSyncScale: {
                    urcurvss.rcu_read_lock(tid);
                    for (int i = 0; i < readLength; i++) sum += readvars[i].load();
                    urcurvss.rcu_read_unlock(tid);
                    break;
                }
                case TwoPhaseEntryPerThread: {
                    int cookie = urcu_tpept.rcu_read_lock(tid);
                    for (int i = 0; i < readLength; i++) sum += readvars[i].load();
                    urcu_tpept.rcu_read_unlock(cookie, tid);
                    break;
                }
                case TwoPhaseAtomicCounterArray: {
                    int cookie = urcu_tpaca.rcu_read_lock();
                    for (int i = 0; i < readLength; i++) sum += readvars[i].load();
                    urcu_tpaca.rcu_read_unlock(cookie);
                    break;
                }
                default:
                    break;
                }
#endif
                numOps++;
            }
            *opsReaders = numOps;
#ifdef LINUX_URCU
            rcu_unregister_thread();
#endif
        };

        // Updater (we measure the ops here)
        auto updater_lambda = [this,&quit,&startFlag,&tc](int tid, long long *ops) {
            long long numOps = 0;
#ifdef LINUX_URCU
            rcu_register_thread();
#endif
            while (!startFlag.load()) this_thread::yield();
            while (!quit.load()) {
#if defined(URCU_BULLET_PROOF_LIB) || defined(LINUX_URCU)
                switch(tc) {
                case BulletProof:
                    ::synchronize_rcu();
                    break;
                }
#else
                switch(tc) {
                case GraceVersion:
                    urcurv.synchronize_rcu();
                    break;
                case GraceVersionSyncScale:
                    urcurvss.synchronize_rcu(tid);
                    break;
                case TwoPhaseEntryPerThread:
                    urcu_tpept.synchronize_rcu();
                    break;
                case TwoPhaseAtomicCounterArray:
                    urcu_tpaca.synchronize_rcu();
                    break;
                }
#endif
                numOps++;
            }
            *ops = numOps;
#ifdef LINUX_URCU
            rcu_unregister_thread();
#endif
        };

        for (int irun = 0; irun < numRuns; irun++) {
            thread readerThreads[2];
            thread updaterThreads[numThreads];
            for (int tid = 0; tid < 2; tid++) readerThreads[tid] = thread(reader_lambda, tid, &opsReaders[tid][irun]);
            for (int tid = 0; tid < numThreads; tid++) updaterThreads[tid] = thread(updater_lambda, tid+2, &ops[tid][irun]);
            startFlag.store(true);
            // Sleep for 100 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < 2; tid++) readerThreads[tid].join();
            for (int tid = 0; tid < numThreads; tid++) updaterThreads[tid].join();
            quit.store(false);
            startFlag.store(false);
        }

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
        }
        vector<long long> aggReaders(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            aggReaders[irun] += opsReaders[0][irun];
            aggReaders[irun] += opsReaders[1][irun];
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        sort(aggReaders.begin(),aggReaders.end());
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        long long result = agg[numRuns/2]/testLengthSeconds.count();
        long long resultReaders = aggReaders[numRuns/2]/testLengthSeconds.count();
        cout << "synchronize_rcu()/sec = " << result << "     readers/sec = " << resultReaders << "\n";
        return result;
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
    	vector<int> threadList = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 30, 32 };
        //vector<int> threadList = { 1, 2, 4 };  // for the laptop
        vector<int> ratioList = { 0, 100 }; // Percentage ratio
        const int numRuns = 5;           // 5 runs for the paper
        const seconds testLength = 20s;   // 20s for the paper

        // Save results
        // [class][ratio][threads]
        long long ops[5][ratioList.size()][threadList.size()];

        for (int ithread = 0; ithread < threadList.size(); ithread++) {
            auto nThreads = threadList[ithread];
            BenchmarkURCU bench(nThreads);
            std::cout << "\n----- URCU Benchmark with 2 Readers    numThreads=" << nThreads << "   numRuns=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined(URCU_BULLET_PROOF_LIB) || defined(LINUX_URCU)
            ops[0][0][ithread] = 0;
            ops[1][0][ithread] = 0;
            ops[2][0][ithread] = 0;
            ops[3][0][ithread] = 0;
            ops[4][0][ithread] = bench.benchmark2Readers(BulletProof, testLength, numRuns);
#else
            ops[0][0][ithread] = bench.benchmark2Readers(GraceVersion, testLength, numRuns);
            ops[1][0][ithread] = bench.benchmark2Readers(GraceVersionSyncScale, testLength, numRuns);
            ops[2][0][ithread] = bench.benchmark2Readers(TwoPhaseEntryPerThread, testLength, numRuns);
            ops[3][0][ithread] = bench.benchmark2Readers(TwoPhaseAtomicCounterArray, testLength, numRuns);
            ops[4][0][ithread] = 0;
#endif
        }

        // Show results in csv format
        cout << "\n\nResults in ops per second for numRuns=" << numRuns << ",  length=" << testLength.count() << "s \n";
#ifdef LINUX_URCU
        cout << "Using Lib URCU\n";
#endif
        cout << "Threads, GraceVersion, GraceVersionSyncScale, TwoPhase, TwoPhase, BulletProof\n";
        for (int ithread = 0; ithread < threadList.size(); ithread++) {
            auto nThreads = threadList[ithread];
            cout << nThreads << ", ";
            for (int il = 0; il < 5; il++) {
                cout << ops[il][0][ithread] << ", ";
            }
            cout << "\n";
        }


        // Now do tests with ratios (only the 0% and 100% matter)
        for (int iratio = 0; iratio < ratioList.size(); iratio++) {
            auto ratio = ratioList[iratio];
            for (int ithread = 0; ithread < threadList.size(); ithread++) {
                auto nThreads = threadList[ithread];
                BenchmarkURCU bench(nThreads);
                std::cout << "\n----- URCU Benchmark   ratio=" << ratio << "%   numThreads=" << nThreads << "   numRuns=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined(URCU_BULLET_PROOF_LIB) || defined(LINUX_URCU)
                ops[0][iratio][ithread] = 0;
                ops[1][iratio][ithread] = 0;
                ops[2][iratio][ithread] = 0;
                ops[3][iratio][ithread] = 0;
                ops[4][iratio][ithread] = bench.benchmark(BulletProof, ratio, testLength, numRuns);
#else
                ops[0][iratio][ithread] = bench.benchmark(GraceVersion, ratio, testLength, numRuns);
                ops[1][iratio][ithread] = bench.benchmark(GraceVersionSyncScale, ratio, testLength, numRuns);
                ops[2][iratio][ithread] = bench.benchmark(TwoPhaseEntryPerThread, ratio, testLength, numRuns);
                ops[3][iratio][ithread] = bench.benchmark(TwoPhaseAtomicCounterArray, ratio, testLength, numRuns);
                ops[4][iratio][ithread] = 0;
#endif
            }
        }

        // Show results in csv format
        cout << "\n\nResults in ops per second for numRuns=" << numRuns << ",  length=" << testLength.count() << "s \n";
#ifdef LINUX_URCU
        cout << "Using Lib URCU\n";
#endif
        for (int iratio = 0; iratio < ratioList.size(); iratio++) {
            auto ratio = ratioList[iratio];
            cout << "Ratio " << ratio << "%\n";
            cout << "Threads, ReadersVersion, ReadersVersionSyncScale, ReadersVersionScale, TwoPhase, BulletProof\n";
            for (int ithread = 0; ithread < threadList.size(); ithread++) {
                auto nThreads = threadList[ithread];
                cout << nThreads << ", ";
                for (int il = 0; il < 5; il++) {
                    cout << ops[il][iratio][ithread] << ", ";
                }
                cout << "\n";
            }
        }

    }
};

#endif
