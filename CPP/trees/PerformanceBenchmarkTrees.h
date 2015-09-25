/*
 * PerformanceTest.h
 *
 *  Created on: Jul 24, 2013
 *      Author: pramalhe
 */

#ifndef _PERFORMANCE_BENCHMARK_TREES_H_
#define _PERFORMANCE_BENCHMARK_TREES_H_

#include <thread>
#include <iostream>
#include <atomic>
#include <forward_list>
#include <algorithm>
#include <vector>
#include <utility>
#include <chrono>
#include <numeric>
#include "TestCasesTrees.h"
#include "LRClassicMap.h"
#include "RIDistributedCacheLineCounter.h"
#include "RWLockPthreadMap.h"
#include "RWLockSharedMutexMap.h"
#include "COWLockMap.h"
#include "LeftRightClassicLambda.h"
//#include "CRWWPSharedMutex.h"

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
const int numElements = 10000;
UserData udarray[numElements];



class PerformanceBenchmarkTrees {

    constexpr static double LINEAR_BIN_WIDTH = 1000000.;      // width of bins is 1 micro-second
    constexpr static long MAX_LINEAR_HIST_BINS = 1000;        // 1000 bins

public:
    // These must be public so that the WorkerThread class instances can access them
    LRClassicMap<int,UserData> lrcAtomicCounterMap;
    LRClassicMap<int,UserData,RIDistributedCacheLineCounter> lrcDCLCMap;
    RWLockPthreadMap<int,UserData> rwlockPthreadMap;
    RWLockSharedMutexMap<int,UserData> rwlockSMMap;
    //LeftRight::LeftRightClassicLambda<std::map<int,UserData>>   lrcLambda {std::map<int,UserData>{}, std::map<int,UserData>{}};
    LeftRight::LeftRightClassicLambda<std::map<int,UserData>>   lrcLambda;
    COWLockMap<int,UserData> cowLockMap;

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
        PerformanceBenchmarkTrees * const pbl;
        const test_case_enum_t testCase = TC_TREES_MAX;
        const int tidx;
        const unsigned int writePerMil = 0;
        const bool measureLatency = false;
        long containsLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];
        long addLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];
        long removeLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];



        WorkerThread(PerformanceBenchmarkTrees * const pbl, const test_case_enum_t testCase,
                     const int tidx, const int writePerMil, const bool measureLatency)
            : pbl(pbl),
              testCase(testCase),
              tidx(tidx),
              writePerMil(writePerMil),
              measureLatency(measureLatency) {
            quit.store(false);
            numOps = 0;
            numReadOps = 0;
            numWriteOps = 0;
            resetHistograms();
        }

        ~WorkerThread() {
            delete th;
        }

        void resetHistograms() {
            for (int i = 0; i < MAX_LINEAR_HIST_BINS; i++) {
                containsLatencyLinearHistogram[i] = 0;
                addLatencyLinearHistogram[i] = 0;
                removeLatencyLinearHistogram[i] = 0;
            }
        }

        void storeContainsLinearLatency(long long deltaNS) {
            int ibox = (int)(deltaNS/1000); // adjust to microseconds
            if (ibox > MAX_LINEAR_HIST_BINS-1) ibox = MAX_LINEAR_HIST_BINS-1;
            containsLatencyLinearHistogram[ibox]++;
        }

        void storeAddLinearLatency(long long deltaNS) {
            int ibox = (int)(deltaNS/1000); // adjust to microseconds
            if (ibox > MAX_LINEAR_HIST_BINS-1) ibox = MAX_LINEAR_HIST_BINS-1;
            addLatencyLinearHistogram[ibox]++;
        }

        void storeRemoveLinearLatency(long long deltaNS) {
            int ibox = (int)(deltaNS/1000); // adjust to microseconds
            if (ibox > MAX_LINEAR_HIST_BINS-1) ibox = MAX_LINEAR_HIST_BINS-1;
            removeLatencyLinearHistogram[ibox]++;
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

            //auto nadaLambda = [](auto _map, auto _key) { return _map->find(_key) != _map->end(); };
            std::function<bool(std::map<int,UserData>*,int)> findLambda =
                [](auto _map, auto _key) { return _map->find(_key) != _map->end(); };
            std::function<bool(std::map<int,UserData>*,int)> eraseLambda =
                [](auto _map, auto _key) { return _map->erase(_key); };
            std::function<bool(std::map<int,UserData>*,std::pair<int,UserData>)> insertLambda =
                [](auto _map, auto _pair) { _map->insert(_pair); return true; };


            // We do it here because auto needs initialization
            auto startBeats = std::chrono::steady_clock::now();
            //auto stopBeats = std::chrono::steady_clock::now();

            while (!quit.load()) {
				xrand = randomLong(xrand);
				x = (xrand < 0) ? (-xrand)%1000 : xrand%1000;
				xrand = randomLong(xrand);
				int i1 = xrand % numElements;
				//UserData& ud1 = udarray[i1];
		        xrand = randomLong(xrand);
                int i2 = xrand % numElements;
		        //UserData& ud2 = udarray[i2];


				if (writePerMil != 1000 && (writePerMil == 0 || x >= writePerMil)) {
					// Read operation
				    if (measureLatency) startBeats = std::chrono::steady_clock::now();
				    switch(testCase) {
				    case TC_TREES_RWL_PT:
                        pbl->rwlockPthreadMap.find(i1);
                        pbl->rwlockPthreadMap.find(i2);
						break;
				    case TC_TREES_RWL_SM:
                        pbl->rwlockSMMap.find(i1);
                        pbl->rwlockSMMap.find(i2);
						break;
                    case TC_TREES_RWL_DCLC:
                        std::cout << "not implemented yet\n";
                        break;
                    case TC_TREES_LRCLASSIC_ATOMIC:
                        pbl->lrcAtomicCounterMap.find(i1);
                        pbl->lrcAtomicCounterMap.find(i2);
                        break;
                    case TC_TREES_LRCLASSIC_DCLC:
                        pbl->lrcDCLCMap.find(i1);
                        pbl->lrcDCLCMap.find(i2);
                        break;
                    case TC_TREES_LRCLASSIC_PER_THREAD:
                        std::cout << "not implemented yet\n";
                        break;
                    case TC_TREES_LRCLASSIC_LAMBDA:
                        pbl->lrcLambda.applyRead( i1, findLambda );
                        pbl->lrcLambda.applyRead( i2, findLambda );
                        break;
                    case TC_TREES_COWLOCK_LRC_ATOMIC:
                        pbl->cowLockMap.find(i1);
                        pbl->cowLockMap.find(i2);
                        break;
                    case TC_TREES_MAX:
                        std::cout << "ERROR\n";
                        break;
				    }
				    if (measureLatency) {
				        auto diff = std::chrono::steady_clock::now()-startBeats;
                        storeContainsLinearLatency(diff.count()/2);
				    }
					numReadOps+=2;
					numOps+=2;
				} else {
					// Write operations
				    switch (testCase) {
                    case TC_TREES_RWL_PT:
                        if (measureLatency) startBeats = std::chrono::steady_clock::now();
                        pbl->rwlockPthreadMap.erase(i1);
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeRemoveLinearLatency(diff.count());
                            startBeats = std::chrono::steady_clock::now();
                        }
                        pbl->rwlockPthreadMap.insert( std::make_pair(i1,udarray[i1]) );
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeAddLinearLatency(diff.count());
                        }
                        break;
                    case TC_TREES_RWL_SM:
                        pbl->rwlockSMMap.erase(i1);
                        pbl->rwlockSMMap.insert( std::make_pair(i1,udarray[i1]) );
                        break;
                    case TC_TREES_RWL_DCLC:
                        std::cout << "not implemented yet\n";
                        break;
                    case TC_TREES_LRCLASSIC_ATOMIC:
                        if (measureLatency) startBeats = std::chrono::steady_clock::now();
                        pbl->lrcAtomicCounterMap.erase(i1);
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeRemoveLinearLatency(diff.count());
                            startBeats = std::chrono::steady_clock::now();
                        }
                        pbl->lrcAtomicCounterMap.insert( std::make_pair(i1,udarray[i1]) );
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeAddLinearLatency(diff.count());
                        }
                        break;
                    case TC_TREES_LRCLASSIC_DCLC:
                        if (measureLatency) startBeats = std::chrono::steady_clock::now();
                        pbl->lrcDCLCMap.erase(i1);
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeRemoveLinearLatency(diff.count());
                            startBeats = std::chrono::steady_clock::now();
                        }
                        pbl->lrcDCLCMap.insert( std::make_pair(i1,udarray[i1]) );
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeAddLinearLatency(diff.count());
                        }
                        break;
                    case TC_TREES_LRCLASSIC_PER_THREAD:
                        std::cout << "not implemented yet\n";
                        break;
                    case TC_TREES_COWLOCK_LRC_ATOMIC:
                        if (measureLatency) startBeats = std::chrono::steady_clock::now();
                        pbl->cowLockMap.erase(i1);
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeRemoveLinearLatency(diff.count());
                            startBeats = std::chrono::steady_clock::now();
                        }
                        pbl->cowLockMap.insert( std::make_pair(i1,udarray[i1]) );
                        if (measureLatency) {
                            auto diff = std::chrono::steady_clock::now()-startBeats;
                            storeAddLinearLatency(diff.count());
                        }
                        break;
                    case TC_TREES_MAX:
                        std::cout << "ERROR\n";
                        break;
                    case TC_TREES_LRCLASSIC_LAMBDA:
                        pbl->lrcLambda.applyMutation( i1, eraseLambda );
                        auto i1pair = std::make_pair(i1,udarray[i1]);
                        pbl->lrcLambda.applyMutation( i1pair, insertLambda );
                        break;
				    }
                    numWriteOps+=2;
					numOps+=2;
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
	PerformanceBenchmarkTrees(int numThreads, int numMilis, int numRuns);
	virtual ~PerformanceBenchmarkTrees();
	void singleTest(test_case_enum_t testCase, int writePerMil);
	void dedicatedTest(test_case_enum_t testCase);
	void singleLatencyTest(test_case_enum_t testCase);
	void addRun(test_case_enum_t testCase, int writePerMil, int numThreads, long opsPerSec);
	void saveDB(int writePerMil);

private:
	const int  _numThreads;
	const long _numMilis;
	const int  _numRuns;
};

#endif /* _PERFORMANCE_BENCHMARK_TREES_H_ */
