/*
 * PerformanceTest.h
 *
 *  Created on: Jul 24, 2013
 *      Author: pramalhe
 */

#ifndef _PERFORMANCE_BENCHMARK_CONSUME_H_
#define _PERFORMANCE_BENCHMARK_CONSUME_H_

#include <thread>
#include <iostream>
#include <atomic>
#include <forward_list>
#include <algorithm>
#include <vector>
#include <utility>
#include "TestCasesConsume.h"
#include "LFLinkedListRCU.h"
#include "LFLinkedListRCUAcquire.h"
#include "LFLinkedListRCUConsume.h"

#define MAX_RUNS  10

class UserData {
public:
    int a;
    int b;
    bool operator==(UserData& l){ return ((l.a == a) && (l.b == b)); }
};

namespace std
{
    template<>
    struct hash<UserData>
    {
        typedef UserData argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type const& u) const
        {
            result_type const h1 ( std::hash<int>()(u.a) );
            result_type const h2 ( std::hash<int>()(u.b) );
            return h1 ^ h2;
        }
    };
}

struct RunEntry {
    test_case_enum_t testCase;
    int writePerMil;
    int numThreads;
    long opsPerSec;
};

std::vector<RunEntry> _runs;

// TODO: Change these to static members of the PerformanceBenchmark class
const int numElements = 100;
UserData udarray[numElements];



class PerformanceBenchmarkConsume {

public:
    // These must be public so that the WorkerThread class instances can access them
    LockFree::LFLinkedListRCU<UserData>        lfllRCU;
    LockFree::LFLinkedListRCUAcquire<UserData> lfllRCUAcquire;
    LockFree::LFLinkedListRCUConsume<UserData> lfllRCUConsume;

    // Forward declaration
    class WorkerThread;
    WorkerThread **_workerThread = nullptr;

	class WorkerThread {
	public:
        std::atomic<bool> quit {false};
        std::atomic<long> aNumOps {0};
        std::atomic<long> aNumReadOps {0};
        std::atomic<long> aNumWriteOps {0};
        long numOps;
        long numReadOps;
        long numWriteOps;
        std::thread * const th = new std::thread(&WorkerThread::run, this);
        PerformanceBenchmarkConsume * const pbl;
        const test_case_enum_t testCase = TC_MMM_MAX;
        const int tidx;
        const unsigned int writePerMil = 0;

        WorkerThread(PerformanceBenchmarkConsume * const pbl, const test_case_enum_t testCase, const int tidx, const int writePerMil)
            : pbl(pbl),
              testCase(testCase),
              tidx(tidx),
              writePerMil(writePerMil) {
              quit.store(false);
        }

        ~WorkerThread() {
            delete th;
        }

        void run() {
            uint64_t xrand = 12345678901234567LL;
            aNumOps.store(0);
            aNumReadOps.store(0);
            numOps            = 0;
            numReadOps        = 0;

            while (!quit.load()) {
				xrand = randomLong(xrand);
				int i1 = xrand % numElements;
				UserData* ud1 = &udarray[i1];

                // Read operation
                switch(testCase) {
                case TC_MMM_RCU:
                    pbl->lfllRCU.contains(ud1);
                    break;
                case TC_MMM_RCU_ACQUIRE:
                    pbl->lfllRCUAcquire.contains(ud1);
                    break;
                case TC_MMM_RCU_CONSUME:
                    pbl->lfllRCUConsume.contains(ud1);
                    break;
                case TC_MMM_HP:
                    std::cout << "ERROR: Not yet implemented\n";
                    break;
                case TC_MMM_MAX:
                    std::cout << "ERROR: Not implemented\n";
                    break;
                }
                numReadOps++;
                numOps++;
			}
			aNumOps.store(numOps);
			aNumReadOps.store(numReadOps);
		}

	private:
		/**
		 * An imprecise but fast random number generator
		 */
		uint64_t randomLong(uint64_t x) {
			x ^= x >> 12; // a
			x ^= x << 25; // b
			x ^= x >> 27; // c
			return x * 2685821657736338717LL;
		}
	};

public:
	PerformanceBenchmarkConsume(int numThreads, int numMilis, int numRuns);
	virtual ~PerformanceBenchmarkConsume();
	void singleTest(test_case_enum_t testCase, int writePerMil);
	void dedicatedTest(test_case_enum_t testCase);
	void addRun(test_case_enum_t testCase, int writePerMil, int numThreads, long opsPerSec);
	void saveDB(int writePerMil);

private:
	const int _numThreads;
	const int _numMilis;
	const int _numRuns;
};

#endif /* _PERFORMANCE_BENCHMARK_TREES_H_ */
