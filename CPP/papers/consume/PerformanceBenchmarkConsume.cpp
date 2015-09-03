/*
 * PerformanceTest.cpp
 *
 *  Created on: Jul 24, 2013
 *      Author: pramalhe
 */

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <algorithm>  // used by std::sort
#include "PerformanceBenchmarkConsume.h"


/*
 */
PerformanceBenchmarkConsume::PerformanceBenchmarkConsume(int numThreads, int numMilis, int numRuns) :
    _numThreads (numThreads), _numMilis (numMilis), _numRuns (numRuns) {
    _workerThread = new WorkerThread*[numThreads];

    std::cout << "Filling up data structures...\n";
    for (int i = 0; i < numElements; i++) { udarray[i].a = i+1; udarray[i].b = (i+3)*2; }
    for (int i = 0; i < numElements; i++) {
        UserData* udp = &udarray[i];
        lfllRCU.add(udp);
        lfllRCUAcquire.add(udp);
        lfllRCUConsume.add(udp);
        // TODO: Add new data structures here
    }
}


PerformanceBenchmarkConsume::~PerformanceBenchmarkConsume() {
    delete[] _workerThread;
    // TODO: empty all the lists
}


/*
 * Mixed tests
 */
void PerformanceBenchmarkConsume::singleTest(test_case_enum_t testCase, int writePerMil) {
	double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
	std::cout << "##### " << test_case_names[testCase] << "  numRuns=" << _numRuns << "   Writes=" << writePercentage << "%   ##### \n";
	std::vector<long long> arrayReadOps(_numRuns);
	std::vector<long long> arrayWriteOps(_numRuns);

	for (int irun = 0; irun < _numRuns; irun++) {
        for (int i = 0; i < _numThreads; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, writePerMil);

        std::chrono::milliseconds dura(_numMilis);
        std::this_thread::sleep_for(dura);

        // Tell the worker threads to stop and join up with the other threads
        for (int i = 0; i < _numThreads; i++) _workerThread[i]->quit.store(true);
        for (int i = 0; i < _numThreads; i++) _workerThread[i]->th->join();
        std::atomic_thread_fence(std::memory_order_seq_cst);

        // Measure the number of performed operations
        arrayReadOps[irun]  = 0;
        arrayWriteOps[irun] = 0;
        for (int i = 0; i < _numThreads; i++) arrayReadOps[irun]  += _workerThread[i]->aNumReadOps.load();
        for (int i = 0; i < _numThreads; i++) arrayWriteOps[irun] += _workerThread[i]->aNumWriteOps.load();

        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (int i = 0; i < _numThreads; i++) delete _workerThread[i];
	}

    // Now compute the median
    int medianRun = _numRuns/2;
    long long medianReads = 0;
    long long medianWrites = 0;
    if (arrayReadOps[0] != 0) {
        std::vector<long long> s(arrayReadOps);
        std::sort(s.begin(), s.end());
        for (int irun = 0; irun < _numRuns; irun++) {
            if (arrayReadOps[irun] == s[medianRun]) {
                medianReads = arrayReadOps[irun];
                medianWrites = arrayWriteOps[irun];
                break;
            }
        }
    } else {
        std::vector<long long> s(arrayWriteOps);
        std::sort(s.begin(), s.end());
        for (int irun = 0; irun < _numRuns; irun++) {
            if (arrayWriteOps[irun] == s[medianRun]) {
                medianReads = arrayReadOps[irun];
                medianWrites = arrayWriteOps[irun];
                break;
            }
        }
    }

    std::cout << "Read Ops/sec = " << (1000LL*medianReads/_numMilis) << "   ";
    std::cout << "Write Ops/sec = " << (1000LL*medianWrites/_numMilis) << "\n";
    // Add the results to the database
    addRun(testCase, writePerMil, _numThreads, (1000LL*(medianReads+medianWrites)/_numMilis));
}


/*
 * Dedicated tests with 2 threads reserved for Writers and the remaining ones for Readers
 */
void PerformanceBenchmarkConsume::dedicatedTest(test_case_enum_t testCase) {
    std::cout << "##### " << test_case_names[testCase] << "   Writers=2   Readers=" << (_numThreads-2) << " ##### \n";
    for(int i = 0; i < 2; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, 1000);
    for(int i = 2; i < _numThreads; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, 0);

    std::chrono::milliseconds dura(_numMilis);
    std::this_thread::sleep_for(dura);

    // Tell the worker threads to stop and join up with the other threads
    for (int i = 0; i < _numThreads; i++) _workerThread[i]->quit.store(true);
    for (int i = 0; i < _numThreads; i++) _workerThread[i]->th->join();
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Measure the number of performed operations
    long long totalNumOps = 0;
    long long totalReadOps = 0;
    long long totalWriteOps = 0;
    for (int i = 0; i < _numThreads; i++) totalNumOps += _workerThread[i]->aNumOps.load();
    for (int i = 0; i < _numThreads; i++) totalReadOps += _workerThread[i]->aNumReadOps.load();
    for (int i = 0; i < _numThreads; i++) totalWriteOps += _workerThread[i]->aNumWriteOps.load();

    std::cout << "Total Ops/sec = " << (1000LL*totalNumOps/_numMilis) << "   ";
    std::cout << "Read Ops/sec = " << (1000LL*totalReadOps/_numMilis) << "   ";
    std::cout << "Write Ops/sec = " << (1000LL*totalWriteOps/_numMilis) << "\n";
    // Add the results to the database
    addRun(testCase, 0, _numThreads, (1000LL*totalNumOps/_numMilis));

    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (int i = 0; i < _numThreads; i++) delete _workerThread[i];
}


void PerformanceBenchmarkConsume::addRun(test_case_enum_t testCase, int writePerMil, int numThreads, long opsPerSec) {
    RunEntry thisRun;
    thisRun.testCase = testCase;
    thisRun.writePerMil = writePerMil;
    thisRun.numThreads = numThreads;
    thisRun.opsPerSec = opsPerSec;
    _runs.push_back(thisRun);
}


void PerformanceBenchmarkConsume::saveDB(int writePerMil) {
    for (int testCase = 0; testCase < TC_MMM_MAX; testCase++) {
        std::cout << test_case_names[testCase] << "  writerPerMil = " << writePerMil << "\n";
        for (auto& run : _runs) {
            if (run.testCase != testCase) continue;
            if (run.writePerMil != writePerMil) continue;
            std::cout << run.numThreads << ", " << run.opsPerSec << "\n";
        }
        std::cout << "\n";
    }
}

void someTests(void) {
    int threadList[] = { 1, 2, 3, 4, 6, 8 };  // To be run on PowerPC
    //int threadList[] = { 1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32, 64 }; // for AWS
    //int threadList[] = { 1, 2, 3, 4, 6, 8, 16, 24, 32 }; // Opteron
    //int threadList[] = { 4 };
    test_case_enum_t testList[] = {
            TC_MMM_RCU,
            TC_MMM_RCU_ACQUIRE,
            TC_MMM_RCU_CONSUME,
    };
    int durationMiliseconds = 20000; // Should be 20 seconds for the final benchmarks
    int numRuns = 5;   // Should be 5 for final benchmarks

    for (unsigned int ithread = 0; ithread < sizeof(threadList)/sizeof(test_case_enum_t); ithread++) {
        PerformanceBenchmarkConsume pf(threadList[ithread], durationMiliseconds, numRuns);
        std::cout << "Number of items = " << numElements << "  and number of threads = " << threadList[ithread] << "\n";
        for (unsigned int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
            pf.singleTest(testList[itest], 0);
            std::cout << "\n";
        }
    }

    std::cout << "\n Duration of tests is " << durationMiliseconds/1000 << " seconds with " << numRuns << " runs\n";
    // We need an instance just to call saveDB
    PerformanceBenchmarkConsume pf(0, 0, 0);
    pf.saveDB(0);     //  0%   -  0%   - 100%
}



int main(void) {
    someTests();
    return 0;
}
