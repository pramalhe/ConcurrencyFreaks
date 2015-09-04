/*
 * PerformanceTest.h
 *
 *  Created on: Jul 24, 2013
 *      Author: pramalhe
 */

#ifndef _PERFORMANCE_BENCHMARK_URCU_H_
#define _PERFORMANCE_BENCHMARK_URCU_H_

#include <thread>
#include <iostream>
#include <atomic>
#include <forward_list>
#include <algorithm>
#include <vector>
#include <utility>
#include "TestCasesURCU.h"
#include "RCUBase.h"
#include "RCUBulletProof.h"
#include "RCUPoorMans.h"

#define MAX_RUNS  10

class UserData {
public:
    int a;
    int b;
    //inline bool operator==(const UserData& l, const UserData& r){ return ((l.a == r.a) && (l.b == r.b)); }
};

struct RunEntry {
    test_case_enum_t testCase;
    int writePerMil;
    int numThreads;
    long opsPerSec;
};

std::vector<RunEntry> _runs;

// TODO: Change these to static members of the PerformanceBenchmark class
const int numElements = 100;
std::atomic<UserData*> udarray[numElements];



class PerformanceBenchmarkURCU {

public:
    // These must be public so that the WorkerThread class instances can access them
    RCU::RCUBulletProof                         _rcuBP;
    RCU::RCUPoorMans                            _rcuPoorMans;
    // TODO: MB


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
        PerformanceBenchmarkURCU * const pbl;
        const test_case_enum_t testCase = TC_RCU_MAX;
        const int tidx;
        const unsigned int writePerMil = 0;
        RCU::RCUBase *rcuBase = nullptr;

        WorkerThread(PerformanceBenchmarkURCU * const pbl, const test_case_enum_t testCase, const int tidx, const int writePerMil)
            : pbl(pbl),
              testCase(testCase),
              tidx(tidx),
              writePerMil(writePerMil) {
              quit.store(false);

              switch(testCase) {
              case TC_RCU_BULLET_PROOF:
                  rcuBase = &pbl->_rcuBP;
                  break;
              case TC_RCU_POOR_MANS:
                  rcuBase = &pbl->_rcuPoorMans;
                  break;
              case TC_RCU_MEMORY_BARRIER:
                  std::cout << "ERROR: Not implemented yet\n";
                  break;
              default:
                  std::cout << "ERROR: Not implemented yet\n";
              }
        }

        ~WorkerThread() {
            delete th;
        }

        void run() {
            unsigned int x = 0;
            uint64_t xrand = 12345678901234567LL;
            aNumOps.store(0);
            aNumReadOps.store(0);
            aNumWriteOps.store(0);
            numOps            = 0;
            numReadOps        = 0;
            numWriteOps       = 0;

            while (!quit.load()) {
				xrand = randomLong(xrand);
				x = (xrand < 0) ? (-xrand)%1000 : xrand%1000;
				xrand = randomLong(xrand);
				const int i1 = xrand % numElements;
				//UserData& ud1 = udarray[i1];

				if (writePerMil != 1000 && (writePerMil == 0 || x >= writePerMil)) {
					// Read operation
				    const int cookie = rcuBase->read_lock();
				    UserData *ud = udarray[i1].load(std::memory_order_relaxed);
				    for (int i = 0; i < numElements; i++) {
				        UserData *udi = udarray[i1].load(std::memory_order_relaxed);
				        // Just some dereferences to cause crashes if there is a bug in
				        // one of the RCU implementations
				        if (ud->a == udi->a && ud->b == udi->b) break;
				    }
				    rcuBase->read_unlock(cookie);
					numReadOps++;
					numOps++;
				} else {
					// Write operations
				    UserData *ud_old = nullptr;
				    UserData *ud_new = new UserData;
                    ud_new->a = ud_new->b = i1;
				    while (true) {
				        // Make a new UserData instance and try to swap one of the current ones with it
				        ud_old = udarray[i1].load();
				        UserData *tmp = ud_old;
				        if (udarray[i1].compare_exchange_strong(tmp, ud_new)) break;
				    }
				    rcuBase->synchronize();
				    // Now it's ok to delete the memory of the older one
				    delete ud_old;
                    numWriteOps++;
					numOps++;
				}
			}
			aNumOps.store(numOps);
			aNumReadOps.store(numReadOps);
			aNumWriteOps.store(numWriteOps);
			//std::cout << " numWriteOps=" << numWriteOps << "\n";
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
	PerformanceBenchmarkURCU(int numThreads, int numMilis, int numRuns);
	virtual ~PerformanceBenchmarkURCU();
	void singleTest(test_case_enum_t testCase, int writePerMil);
	void addRun(test_case_enum_t testCase, int writePerMil, int numThreads, long opsPerSec);
	void saveDB(int writePerMil);

private:
	const int _numThreads;
	const int _numMilis;
	const int _numRuns;
};

#endif /* _PERFORMANCE_BENCHMARK_TREES_H_ */
