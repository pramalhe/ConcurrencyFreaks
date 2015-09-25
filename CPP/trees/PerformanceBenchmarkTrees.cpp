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
#include "PerformanceBenchmarkTrees.h"


/*
 */
PerformanceBenchmarkTrees::PerformanceBenchmarkTrees(int numThreads, int numMilis, int numRuns) :
    _numThreads (numThreads), _numMilis (numMilis), _numRuns (numRuns) {
    _workerThread = new WorkerThread*[numThreads];

    std::cout << "Filling up data structures...\n";
    for (int i = 0; i < numElements; i++) udarray[i].a = i;
    for (int i = 0; i < numElements; i++) {
        UserData& udp = udarray[i];
        lrcAtomicCounterMap.insert( std::pair<int,UserData>(i,udp) );
        lrcDCLCMap.insert( std::pair<int,UserData>(i,udp) );
        rwlockPthreadMap.insert( std::pair<int,UserData>(i,udp) );
        //rwlockSMMap.insert( std::pair<int,UserData>(i,udp) ); // This causes weird bugs
        cowLockMap.insert( std::pair<int,UserData>(i,udp) );
        // Lambdas are tricky
        std::function<bool(std::map<int,UserData>*,std::pair<int,UserData>)> insertLambda =
            [](std::map<int,UserData>* _map, std::pair<int,UserData> _pair) { _map->insert(_pair); return true; };
        auto ipair = std::make_pair(i,udarray[i]);
        lrcLambda.applyMutation<bool,std::pair<int,UserData>>( ipair, insertLambda );

        // TODO: Add new data structures here
    }
}


PerformanceBenchmarkTrees::~PerformanceBenchmarkTrees() {
    delete[] _workerThread;
    // TODO: empty all the lists
}


/*
 * Mixed tests
 */
void PerformanceBenchmarkTrees::singleTest(test_case_enum_t testCase, int writePerMil) {
	double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
	std::cout << "##### " << test_case_names[testCase] << "  numRuns=" << _numRuns << "   Writes=" << writePercentage << "%   ##### \n";
	std::vector<long long> arrayReadOps(_numRuns);
	std::vector<long long> arrayWriteOps(_numRuns);

	for (int irun = 0; irun < _numRuns; irun++) {
        for (int i = 0; i < _numThreads; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, writePerMil, false);

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
void PerformanceBenchmarkTrees::dedicatedTest(test_case_enum_t testCase) {
    std::cout << "##### " << test_case_names[testCase] << "   Writers=2   Readers=" << (_numThreads-2) << " ##### \n";
    for(int i = 0; i < 2; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, 1000, false);
    for(int i = 2; i < _numThreads; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i, 0, false);

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


/**
 * WARNING: make sure to set 1000*1000 millis and 10000 items
 */
void PerformanceBenchmarkTrees::singleLatencyTest(test_case_enum_t testCase) {
    // Create 1 thread for writing and 4 threads for reading
    const int threadsUsed = 5;
    std::cout << "##### " << test_case_names[testCase] << "   Writers=1   Readers=4 ##### \n";
    _workerThread[0] = new WorkerThread(this, testCase, 0, 1000, true);
    _workerThread[1] = new WorkerThread(this, testCase, 1, 0, true);
    _workerThread[2] = new WorkerThread(this, testCase, 2, 0, true);
    _workerThread[3] = new WorkerThread(this, testCase, 3, 0, true);
    _workerThread[4] = new WorkerThread(this, testCase, 4, 0, true);

    std::chrono::milliseconds dura(_numMilis);
    std::this_thread::sleep_for(dura);

    // Tell the worker threads to stop and join up with the other threads
    for (int i = 0; i < threadsUsed; i++) _workerThread[i]->quit.store(true);
    for (int i = 0; i < threadsUsed; i++) _workerThread[i]->th->join();
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Create the normalized histograms and reset them
    double containsLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];
    double addLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];
    double removeLatencyLinearHistogram[MAX_LINEAR_HIST_BINS];
    for (int i = 0; i < MAX_LINEAR_HIST_BINS; i++) {
        containsLatencyLinearHistogram[i] = 0;
        addLatencyLinearHistogram[i] = 0;
        removeLatencyLinearHistogram[i] = 0;
    }

    // Aggregate the CONTAINS histograms from all the Reader threads
    for (int i = 0; i < threadsUsed; i++) {
        for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
            containsLatencyLinearHistogram[ibox] += _workerThread[i]->containsLatencyLinearHistogram[ibox];
        }
    }
    // Normalize the histograms
    double normFactor = 0;
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        normFactor += containsLatencyLinearHistogram[ibox];
    }
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        containsLatencyLinearHistogram[ibox] /= normFactor;
    }
    // Show the histograms in csv format
    std::cout <<"--------------------- CONTAINS -----------------\n";
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        std::cout << ibox << ", " << containsLatencyLinearHistogram[ibox] << "\n";
    }

    // Aggregate the ADD histograms from all the Writer threads
    for (int i = 0; i < threadsUsed; i++) {
        for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
            addLatencyLinearHistogram[ibox] += _workerThread[i]->addLatencyLinearHistogram[ibox];
        }
    }
    // Normalize the histograms
    normFactor = 0;
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        normFactor += addLatencyLinearHistogram[ibox];
    }
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        addLatencyLinearHistogram[ibox] /= normFactor;
    }
    // Show the histograms in csv format
    std::cout << "--------------------- ADD -----------------\n";
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        std::cout << ibox << ", " << addLatencyLinearHistogram[ibox] << "\n";
    }

    // Aggregate the REMOVE histograms from all the Writer threads
    for (int i = 0; i < threadsUsed; i++) {
        for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
            removeLatencyLinearHistogram[ibox] += _workerThread[i]->removeLatencyLinearHistogram[ibox];
        }
    }
    // Normalize the histograms
    normFactor = 0;
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        normFactor += removeLatencyLinearHistogram[ibox];
    }
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        removeLatencyLinearHistogram[ibox] /= normFactor;
    }
    // Show the histograms in csv format
    std::cout << "--------------------- REMOVE -----------------\n";
    for (int ibox = 0 ; ibox < MAX_LINEAR_HIST_BINS; ibox++) {
        std::cout << ibox << ", " << removeLatencyLinearHistogram[ibox] << "\n";
    }

    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (int i = 0; i < threadsUsed; i++) delete _workerThread[i];
}


void PerformanceBenchmarkTrees::addRun(test_case_enum_t testCase, int writePerMil, int numThreads, long opsPerSec) {
    RunEntry thisRun;
    thisRun.testCase = testCase;
    thisRun.writePerMil = writePerMil;
    thisRun.numThreads = numThreads;
    thisRun.opsPerSec = opsPerSec;
    _runs.push_back(thisRun);
}


void PerformanceBenchmarkTrees::saveDB(int writePerMil) {
    for (int testCase = 0; testCase < TC_TREES_MAX; testCase++) {
        std::cout << test_case_names[testCase] << "  writerPerMil = " << writePerMil << "\n";
        for (auto& run : _runs) {
            if (run.testCase != testCase) continue;
            if (run.writePerMil != writePerMil) continue;
            std::cout << run.numThreads << ", " << run.opsPerSec << "\n";
        }
        std::cout << "\n";
    }
}


/*
 * Run mixed tests for CppCon 2015
 */
void cppcon2015Mixed(void) {
    //int threadList[] = { 1, 2, 4, 8, 16, 24, 32 };
    int threadList[] = { 4 };
    test_case_enum_t testList[] = {
        //TEST_CASE_TREES_RWL_PT,
        //TEST_CASE_TREES_RWLL_PT, TEST_CASE_TREES_RWLL_SM, TEST_CASE_TREES_RWLL_DCLC,
        TC_TREES_LRCLASSIC_ATOMIC, //TEST_CASE_TREES_LRCLASSIC_DCLC,
    };
    int durationMiliseconds = 10000; // 30 seconds per test
    int numRuns = 1;   // Should be 5 for final benchmarks
    std::cout << "CppCon 2015 Mixed benchmarks with a duration=" << (durationMiliseconds/1000) << " seconds\n\n";
    for (unsigned int ithread = 0; ithread < sizeof(threadList)/sizeof(int); ithread++) {
        PerformanceBenchmarkTrees pf(threadList[ithread], durationMiliseconds, numRuns);
        std::cout << "Number of items = " << numElements << "  and number of threads = " << threadList[ithread] << "\n";

        for (unsigned int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
            pf.singleTest(testList[itest], 1000);
            pf.singleTest(testList[itest], 100);
            pf.singleTest(testList[itest], 10);
            pf.singleTest(testList[itest], 0);
            std::cout << "\n";
        }
    }

    std::cout << "\n Duration of tests is " << durationMiliseconds/1000 << " seconds \n";
    // We need an instance just to call saveDB
    PerformanceBenchmarkTrees pf(0, 0, 0);
    pf.saveDB(1000);  // 50%   - 50%   -   0%
    pf.saveDB(100);   //  5%   -  5%   -  90%
    pf.saveDB(10);    //  0.5% -  0.5% -  99%
    pf.saveDB(0);     //  0%   -  0%   - 100%
}


/*
 * Latency tests for CppCon
 */
void cppcon2015Latency(void) {
    test_case_enum_t testList[] = {
        TC_TREES_RWL_PT,
        TC_TREES_LRCLASSIC_ATOMIC,
        TC_TREES_LRCLASSIC_DCLC,
        TC_TREES_COWLOCK_LRC_ATOMIC,
    };
    long durationMiliseconds = 10000L; // Final tests should be 1000 seconds per test
    std::cout << "CppCon 2015 Latency (1+4 threads) with a duration=" << (durationMiliseconds/1000) << " seconds\n\n";
    for (unsigned int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
        PerformanceBenchmarkTrees pf(5, durationMiliseconds, 0);
        pf.singleLatencyTest(testList[itest]);
    }
}


/*
 * Run dedicated tests for CppCon 2015
 */
void cppcon2015Dedicated(void) {

    //int threadList[] = { 1, 2, 4, 8, 16, 24, 32 };
    int threadList[] = { 4 };
    test_case_enum_t testList[] = {
        //TEST_CASE_TREES_RWL_PT,
        //TEST_CASE_TREES_RWLL_PT, TEST_CASE_TREES_RWLL_SM, TEST_CASE_TREES_RWLL_DCLC,
        TC_TREES_LRCLASSIC_ATOMIC, //TEST_CASE_TREES_LRCLASSIC_DCLC,
    };
    long durationMiliseconds = 10000L; // Should be 10 seconds
    int numRuns = 1;   // Should be 5 for final benchmarks
    std::cout << "CppCon 2015 Mixed benchmarks with a duration=" << (durationMiliseconds/1000) << " seconds\n\n";
    for (unsigned int ithread = 0; ithread < sizeof(threadList)/sizeof(int); ithread++) {
        PerformanceBenchmarkTrees pf(threadList[ithread], durationMiliseconds, numRuns);
        std::cout << "Number of items = " << numElements << "  and number of threads = " << threadList[ithread] << "\n";

        for (unsigned int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
            pf.singleTest(testList[itest], 1000);
            pf.singleTest(testList[itest], 100);
            pf.singleTest(testList[itest], 10);
            pf.singleTest(testList[itest], 0);
            std::cout << "\n";
        }
    }

    std::cout << "\n Duration of tests is " << durationMiliseconds/1000 << " seconds \n";
}



void someTests(void) {
    int threadList[] = { 1, 2, 4, 8 };
    //int threadList[] = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 64 }; // for AWS
    //int threadList[] = { 1, 2, 4, 8, 16, 24, 32, 64 };
    //int threadList[] = { 4 };
    test_case_enum_t testList[] = {
        TC_TREES_LRCLASSIC_LAMBDA,
        TC_TREES_RWL_PT,
        TC_TREES_LRCLASSIC_ATOMIC, TC_TREES_LRCLASSIC_DCLC,
        TC_TREES_COWLOCK_LRC_ATOMIC,
    };
    int durationMiliseconds = 10000; // 10 seconds per test
    int numRuns = 1;   // Should be 5 for final benchmarks

    for (unsigned int ithread = 0; ithread < sizeof(threadList)/sizeof(int); ithread++) {
        PerformanceBenchmarkTrees pf(threadList[ithread], durationMiliseconds, numRuns);
        std::cout << "Number of items = " << numElements << "  and number of threads = " << threadList[ithread] << "\n";
        for (unsigned int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
            pf.singleTest(testList[itest], 1000);
            pf.singleTest(testList[itest], 100);
            pf.singleTest(testList[itest], 10);
            //pf.singleTest(testList[itest], 1);
            pf.singleTest(testList[itest], 0);
            std::cout << "\n";
        }
    }

    std::cout << "\n Duration of tests is " << durationMiliseconds/1000 << " seconds \n";
    // We need an instance just to call saveDB
    PerformanceBenchmarkTrees pf(0, 0, 0);
    pf.saveDB(1000);  // 50%   - 50%   -   0%
    pf.saveDB(100);   //  5%   -  5%   -  90%
    pf.saveDB(10);    //  0.5% -  0.5% -  99%
    //pf.saveDB(1);
    pf.saveDB(0);     //  0%   -  0%   - 100%
}



int main(void) {
    someTests();
    //cppcon2015Mixed();
    //cppcon2015Dedicated();
    //cppcon2015Latency(); // WARNING: set numElements to 10000
    return 0;
}
