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
#ifndef _BENCHMARK_Q_H_
#define _BENCHMARK_Q_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include "MichaelScottQueue.hpp"
#include "CRTurnQueue.hpp"
//#include "KoganPetrankQueueCHP.hpp"


using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark to run the tests shown in CRTurnQueue paper
 *
 *
 */
class BenchmarkQ {

private:

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

    // Performance benchmark constants
    static const long long kNumPairsWarmup =     1000000LL;     // Each threads does 1M iterations as warmup

    // Contants for Ping-Pong performance benchmark
    static const int kPingPongBatch = 1000;            // Each thread starts by injecting 1k items in the queue


    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
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
    };

    BenchmarkQ(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * enqueue-dequeue pairs: in each iteration a thread executes an enqueue followed by a dequeue;
     * the benchmark executes 10^8 pairs partitioned evenly among all threads;
     */
    template<typename Q>
    void enqDeqBenchmark(const long numPairs, const int numRuns) {
        nanoseconds deltas[numThreads][numRuns];
        atomic<bool> startFlag = { false };
        Q* queue = nullptr;

        auto enqdeq_lambda = [this,&startFlag,&numPairs,&queue](nanoseconds *delta, const int tid) {
            UserData ud(0,0);
            while (!startFlag.load()) {} // Spin until the startFlag is set
            // Warmup phase
            for (long long iter = 0; iter < kNumPairsWarmup/numThreads; iter++) {
                queue->enqueue(&ud, tid);
                if (queue->dequeue(tid) == nullptr) cout << "Error at warmup dequeueing iter=" << iter << "\n";
            }
            // Measurement phase
            auto startBeats = steady_clock::now();
            for (long long iter = 0; iter < numPairs/numThreads; iter++) {
                queue->enqueue(&ud, tid);
                if (queue->dequeue(tid) == nullptr) cout << "Error at measurement dequeueing iter=" << iter << "\n";
            }
            auto stopBeats = steady_clock::now();
            *delta = stopBeats - startBeats;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            queue = new Q(numThreads);
            if (irun == 0) cout << "##### " << queue->className() << " #####  \n";
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(enqdeq_lambda, &deltas[tid][irun], tid);
            startFlag.store(true);
            // Sleep for 2 seconds just to let the threads see the startFlag
            this_thread::sleep_for(2s);
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            startFlag.store(false);
            delete (Q*)queue;
        }

        // Sum up all the time deltas of all threads so we can find the median run
        vector<nanoseconds> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0ns;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += deltas[tid][irun];
            }
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        const long long NSEC_IN_SEC = 1000000000LL;
        auto median = agg[numRuns/2].count()/numThreads; // Normalize back to per-thread time (mean of time for this run)

        cout << "Total Ops/sec = " << numPairs*2*NSEC_IN_SEC/median << "\n";
    }



    /**
     * Start with only enqueues 100K/numThreads, wait for them to finish, then do only dequeues but only 100K/numThreads
     */
    template<typename Q>
    void burstBenchmark(const long long burstSize, const int numIters, const int numRuns) {
        Result results[numThreads][numRuns];
        atomic<bool> startEnq = { false };
        atomic<bool> startDeq = { false };
        atomic<long> barrier = { 0 };
        Q* queue = nullptr;

        auto burst_lambda = [this,&startEnq,&startDeq,&burstSize,&barrier,&numIters,&queue](Result *res, const int tid) {
            UserData ud(0,0);

            // Warmup
            const long long warmupIters = 100000LL;  // Do 1M for each thread as a warmup
            for (long long iter = 0; iter < warmupIters; iter++) queue->enqueue(&ud, tid);
            for (long long iter = 0; iter < warmupIters; iter++) {
                if (queue->dequeue(tid) == nullptr) cout << "ERROR: warmup dequeued nullptr in iter=" << iter << "\n";
            }
            // Measurements
            for (int iter=0; iter < numIters; iter++) {
                // Start with enqueues
                while (!startEnq.load()) this_thread::yield();
                auto startBeats = steady_clock::now();
                for (long long iter = 0; iter < burstSize/numThreads; iter++) {
                    queue->enqueue(&ud, tid);
                }
                auto stopBeats = steady_clock::now();
                res->nsEnq += (stopBeats-startBeats);
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
                // dequeues
                while (!startDeq.load()) this_thread::yield();
                startBeats = steady_clock::now();
                for (long long iter = 0; iter < burstSize/numThreads; iter++) {
                    if (queue->dequeue(tid) == nullptr) {
                        cout << "ERROR: dequeued nullptr in iter=" << iter << "\n";
                        assert(false);
                    }
                }
                stopBeats = steady_clock::now();
                res->nsDeq += (stopBeats-startBeats);
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
                res->numEnq += burstSize/numThreads;
                res->numDeq += burstSize/numThreads;
            }
        };

        auto startAll = steady_clock::now();
        for (int irun = 0; irun < numRuns; irun++) {
            queue = new Q(numThreads);
            if (irun == 0) cout << "##### " << queue->className() << " #####  \n";
            thread burstThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) burstThreads[tid] = thread(burst_lambda, &results[tid][irun], tid);
            this_thread::sleep_for(100ms);
            for (int iter=0; iter < numIters; iter++) {
                // enqueue round
                startEnq.store(true);
                while (barrier.load() != numThreads) this_thread::yield();
                startEnq.store(false);
                long tmp =  numThreads;
                if (!barrier.compare_exchange_strong(tmp, 0)) cout << "ERROR: CAS\n";
                // dequeue round
                startDeq.store(true);
                while (barrier.load() != numThreads) this_thread::yield();
                startDeq.store(false);
                tmp = numThreads;
                if (!barrier.compare_exchange_strong(tmp, 0)) cout << "ERROR: CAS\n";
            }
            for (int tid = 0; tid < numThreads; tid++) burstThreads[tid].join();
            delete queue;
        }
        auto endAll = steady_clock::now();
        milliseconds totalMs = duration_cast<milliseconds>(endAll-startAll);

        // Accounting
        vector<Result> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun].nsEnq += results[tid][irun].nsEnq;
                agg[irun].nsDeq += results[tid][irun].nsDeq;
                agg[irun].numEnq += results[tid][irun].numEnq;
                agg[irun].numDeq += results[tid][irun].numDeq;
            }
            agg[irun].totOpsSec = (agg[irun].numEnq+agg[irun].numDeq)*NSEC_IN_SEC/(agg[irun].nsEnq.count()+agg[irun].nsDeq.count());
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        Result median = agg[numRuns/2];
        const long long NSEC_IN_SEC = 1000000000LL;
        const long long allThreadsEnqPerSec = numThreads*median.numEnq*NSEC_IN_SEC/median.nsEnq.count();
        const long long allThreadsDeqPerSec = numThreads*median.numDeq*NSEC_IN_SEC/median.nsDeq.count();

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        cout << "Enq/sec = " << allThreadsEnqPerSec << "   Deq/sec = " << allThreadsDeqPerSec <<
                "   Total = " << (median.numEnq+median.numDeq) << "   Ops/sec = " << median.totOpsSec << "\n";

        // TODO: Print csv values
    }


    template<typename Q>
    void pingPongBenchmark(const seconds testLengthSeconds, const int numRuns) {
        Result results[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        Q* queue = nullptr;

        auto pingpong_lambda = [&quit,&startFlag,&queue](Result *res, const int tid) {
            UserData ud(0,0);
            nanoseconds nsEnq = 0ns;
            nanoseconds nsDeq = 0ns;
            long long numEnq = 0;
            long long numDeq = 0;
            while (!startFlag.load()) this_thread::yield();
            while (!quit.load()) {
                // Always do kPingPongBatch (1k) enqueues and measure the time
                auto startBeats = steady_clock::now();
                for (int i = 0; i < kPingPongBatch; i++) { queue->enqueue(&ud, tid); /*this_thread::sleep_for(50ms);*/ }
                auto stopBeats = steady_clock::now();
                numEnq += kPingPongBatch;
                nsEnq += (stopBeats-startBeats);
                // Do dequeues until queue is empty, measure the time, and count how many were non-null
                startBeats = steady_clock::now();
                stopBeats = startBeats;
                while (queue->dequeue(tid) != nullptr) {
                	numDeq++; /*this_thread::sleep_for(50ms);*/
                	stopBeats = steady_clock::now();
                }
                nsDeq += (stopBeats-startBeats);
            }
            res->nsEnq = nsEnq;
            res->nsDeq = nsDeq;
            res->numEnq = numEnq;
            res->numDeq = numDeq;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            queue = new Q(numThreads);
            if (irun == 0) cout << "##### " << queue->className() << " #####  \n";
            thread pingpongThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) pingpongThreads[tid] = thread(pingpong_lambda, &results[tid][irun], tid);
            startFlag.store(true);
            // Sleep for 20 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) pingpongThreads[tid].join();
            quit.store(false);
            startFlag.store(false);
            delete queue;
        }

        // Accounting
        vector<Result> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun].nsEnq += results[tid][irun].nsEnq;
                agg[irun].nsDeq += results[tid][irun].nsDeq;
                agg[irun].numEnq += results[tid][irun].numEnq;
                agg[irun].numDeq += results[tid][irun].numDeq;
            }
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        Result median = agg[numRuns/2];
        const long long NSEC_IN_SEC = 1000000000LL;
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        cout << "Enq/sec=" << numThreads*median.numEnq*NSEC_IN_SEC/median.nsEnq.count() << "   Deq/sec=" << numThreads*median.numDeq*NSEC_IN_SEC/median.nsDeq.count() << "   Total=" << numThreads*(median.numEnq+median.numDeq)<< "\n";
    }





public:

    static void allThroughputTests() {
        vector<int> threadList = { 1, 2, 4, 8, 16, 24, 32 };
        const int numRuns = 5;           // 5 runs for the paper

        // Enq-Deq Throughput benchmarks
        for (int nThreads : threadList) {
            const int numPairs = 100*1000000LL;
            BenchmarkQ bench(nThreads);
            std::cout << "\n----- Enq-Deq Benchmark   numThreads=" << nThreads << "   numPairs=" << numPairs/1000000LL << "M -----\n";
            bench.enqDeqBenchmark<MichaelScottQueue<UserData>>(numPairs, numRuns);
            //bench.enqDeqBenchmark<KoganPetrankQueueCHP<UserData>>(numPairs, numRuns);
            bench.enqDeqBenchmark<CRTurnQueue<UserData>>(numPairs, numRuns);
        }

        // Burst Throughput benchmarks
        const long long burstSize = 1000000LL;     // 1M for the paper
        const int numIters = 100;                  // Number of iterations of 1M enqueues/dequeues
        for (int nThreads : threadList) {
            BenchmarkQ bench(nThreads);
            std::cout << "\n----- Burst Benchmark   numThreads=" << nThreads << "   burstSize=" << burstSize/1000LL << "K   numIters=" << numIters << " -----\n";
            bench.burstBenchmark<MichaelScottQueue<UserData>>(burstSize, numIters, numRuns);
            //bench.burstBenchmark<KoganPetrankQueueCHP<UserData>>(burstSize, numIters, numRuns);
            bench.burstBenchmark<CRTurnQueue<UserData>>(burstSize, numIters, numRuns);
        }
    }
};

#endif
