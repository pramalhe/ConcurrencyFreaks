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
#ifndef _BENCHMARK_LATENCY_Q_H_
#define _BENCHMARK_LATENCY_Q_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>

// Queues:
#include "MichaelScottQueue.hpp"
#include "CRTurnQueue.hpp"
//#include "KoganPetrankQueueCHP.hpp"


using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark for Latency tests to show in CRTurnQueue paper
 *
 * <h2> Latency Distribution </h2>
 *  TODO:
 *
 * - We fire up 30 threads of type LatencyThread;
 * - Each thread does alternatively 1000 enqueue() and 1000 dequeue(). All dequeues are non-null;
 * - After start, each thread does 1M iterations as warmup.
 * - Measurements are done for 4M iterations, that are saved in a local array, 2M enqueue and 2M dequeue;
 * -
 *
 */
class BenchmarkLatencyQ {

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

    // Latency constants
    static const long long kLatencyMeasures =     200000000LL;   // We measure 200M iterations divided among the different threads
    static const long long kLatencyWarmupIterations =    10;     // At start of latency tests we do 10M warmup enqueues and dequeues in bursts of 100K
    static const long long kLatencyIterations =         200;     // We do this many iterations of 100K enqueues and dequeues until we get kLatencyMeasures
    static const long long kLatencyBurst = kLatencyMeasures/kLatencyIterations;

    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;
    int numRuns;
    seconds testLengthSeconds;

public:
    BenchmarkLatencyQ(int numThreads, int numRuns, seconds testLength) {
        this->numThreads = numThreads;
        this->numRuns = numRuns;
        this->testLengthSeconds = testLength;
    }

	
    /*
     * Execute latency benchmarks
     * Make sure to enable high priority for the Windows process
     *
     * We only do one run for this benchmark
     */
    template<typename Q>
    void latencyBurstBenchmark() {
        atomic<bool> startEnq = { false };
        atomic<bool> startDeq = { false };
        atomic<long> barrier = { 0 };
        Q* queue = new Q(numThreads);

        auto latency_lambda = [this,&startEnq,&startDeq,&barrier,&queue](nanoseconds* enqDelays, nanoseconds* deqDelays, const int tid) {
            UserData ud(0,0);
            long long enqDelayIndex = 0;
            long long deqDelayIndex = 0;

            // Warmup + Measurements
            for (int iter=0; iter < kLatencyIterations+kLatencyWarmupIterations; iter++) {
                // Start with enqueues
                while (!startEnq.load()) this_thread::yield();
                for (long long i = 0; i < kLatencyBurst/numThreads; i++) {
                    auto startBeats = steady_clock::now();
                    queue->enqueue(&ud, tid);
                    auto stopBeats = steady_clock::now();
                    if (iter >= kLatencyWarmupIterations) enqDelays[enqDelayIndex++] = (stopBeats-startBeats);
                }
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
                // dequeues
                while (!startDeq.load()) this_thread::yield();
                for (long long i = 0; i < kLatencyBurst/numThreads; i++) {
                    auto startBeats = steady_clock::now();
                    if (queue->dequeue(tid) == nullptr) cout << "ERROR: dequeued nullptr in i=" << i << "\n";
                    auto stopBeats = steady_clock::now();
                    if (iter >= kLatencyWarmupIterations) deqDelays[deqDelayIndex++] = (stopBeats-startBeats);
                }
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
            }
        };

        nanoseconds* enqDelays[numThreads];  // Half enqueues and half dequeues
        nanoseconds* deqDelays[numThreads];
        for (int it = 0; it < numThreads; it++) {
            enqDelays[it] = new nanoseconds[kLatencyMeasures/numThreads];
            deqDelays[it] = new nanoseconds[kLatencyMeasures/numThreads];
            for (int imeas=0; imeas < kLatencyMeasures/numThreads; imeas++) {
                enqDelays[it][imeas] = 0ns;
                deqDelays[it][imeas] = 0ns;
            }
        }

        cout << "##### " << queue->className() << " #####  \n";
        thread latencyThreads[numThreads];
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid] = thread(latency_lambda, enqDelays[tid], deqDelays[tid], tid);
        this_thread::sleep_for(50ms);
        for (int iter=0; iter < kLatencyIterations+kLatencyWarmupIterations; iter++) {
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
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid].join();
        delete queue;

        // Aggregate all the delays for enqueues and dequeues and compute the maxs
        cout << "Aggregating delays for " << kLatencyMeasures/1000000 << " million measurements...\n";
        vector<nanoseconds> aggEnqDelay(kLatencyMeasures);
        long long idx = 0;
        for (int it = 0; it < numThreads; it++) {
            for (int i = 0; i < kLatencyMeasures/numThreads; i++) {
                aggEnqDelay[idx] = enqDelays[it][i];
                idx++;
            }
        }
        vector<nanoseconds> aggDeqDelay(kLatencyMeasures);
        idx = 0;
        for (int it = 0; it < numThreads; it++) {
            for (int i = 0; i < kLatencyMeasures/numThreads; i++) {
                aggDeqDelay[idx] = deqDelays[it][i];
                idx++;
            }
        }

        // Sort the aggregated delays
        cout << "Sorting delays...\n";
        sort(aggEnqDelay.begin(), aggEnqDelay.end());
        sort(aggDeqDelay.begin(), aggDeqDelay.end());

        // Show the 50% (median), 90%, 99%, 99.9%, 99.99%, 99.999% and maximum in microsecond/nanoseconds units
        long per50000 = (long)(kLatencyMeasures*50000LL/100000LL);
        long per70000 = (long)(kLatencyMeasures*70000LL/100000LL);
        long per80000 = (long)(kLatencyMeasures*80000LL/100000LL);
        long per90000 = (long)(kLatencyMeasures*90000LL/100000LL);
        long per99000 = (long)(kLatencyMeasures*99000LL/100000LL);
        long per99900 = (long)(kLatencyMeasures*99900LL/100000LL);
        long per99990 = (long)(kLatencyMeasures*99990LL/100000LL);
        long per99999 = (long)(kLatencyMeasures*99999LL/100000LL);
        long imax = kLatencyMeasures-1;

        cout << "Enqueue delay (us): 50%=" << aggEnqDelay[per50000].count()/1000 << "  70%=" << aggEnqDelay[per70000].count()/1000 << "  80%=" << aggEnqDelay[per80000].count()/1000
             << "  90%=" << aggEnqDelay[per90000].count()/1000 << "  99%=" << aggEnqDelay[per99000].count()/1000
             << "  99.9%=" << aggEnqDelay[per99900].count()/1000 << "  99.99%=" << aggEnqDelay[per99990].count()/1000
             << "  99.999%=" << aggEnqDelay[per99999].count()/1000 << "  max=" << aggEnqDelay[imax].count()/1000 << "\n";
        cout << "Dequeue delay (us): 50%=" << aggDeqDelay[per50000].count()/1000 << "  70%=" << aggDeqDelay[per70000].count()/1000 << "  80%=" << aggDeqDelay[per80000].count()/1000
             << "  90%=" << aggDeqDelay[per90000].count()/1000 << "  99%=" << aggDeqDelay[per99000].count()/1000
             << "  99.9%=" << aggDeqDelay[per99900].count()/1000 << "  99.99%=" << aggDeqDelay[per99990].count()/1000
             << "  99.999%=" << aggDeqDelay[per99999].count()/1000 << "  max=" << aggDeqDelay[imax].count()/1000 << "\n";

        // Show in csv format
        cout << "Enqueue delay (us):\n";
        cout << "50, " << aggEnqDelay[per50000].count()/1000 << "\n";
        cout << "90, " << aggEnqDelay[per90000].count()/1000 << "\n";
        cout << "99, " << aggEnqDelay[per99000].count()/1000 << "\n";
        cout << "99.9, " << aggEnqDelay[per99900].count()/1000 << "\n";
        cout << "99.99, " << aggEnqDelay[per99990].count()/1000 << "\n";
        cout << "99.999, " << aggEnqDelay[per99999].count()/1000 << "\n";
        cout << "Dequeue delay (us):\n";
        cout << "50, " << aggDeqDelay[per50000].count()/1000 << "\n";
        cout << "90, " << aggDeqDelay[per90000].count()/1000 << "\n";
        cout << "99, " << aggDeqDelay[per99000].count()/1000 << "\n";
        cout << "99.9, " << aggDeqDelay[per99900].count()/1000 << "\n";
        cout << "99.99, " << aggDeqDelay[per99990].count()/1000 << "\n";
        cout << "99.999, " << aggDeqDelay[per99999].count()/1000 << "\n";

        // Cleanup
        for (int it = 0; it < numThreads; it++) {
            delete[] enqDelays[it];
            delete[] deqDelays[it];
        }
    }



public:

    static void allLatencyTests() {
        // Burst Latency benchmarks
        //vector<int> threadList = { 30, 30, 30, 30, 30, 30, 30 }; // For the latency table in the paper
        vector<int> threadList = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 30, 32 };
        
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<MichaelScottQueue<UserData>>();
        }
        //for (int nThreads : threadList) {
        //    BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
        //    std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
        //    bench.latencyBurstBenchmark<KoganPetrankQueueCHP<UserData>>();
        //}
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<CRTurnQueue<UserData>>();
        }        
    }
};

#endif
