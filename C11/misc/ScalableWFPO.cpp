/******************************************************************************
 * Copyright (c) 2014, Pedro Ramalhete, Andreia Correia
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

/*
 * Compile this with:
 * gcc -O3 -std=c++14 ScalableWFPO.cpp -o swfpo.exe -lstdc++
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

/*
 * Types, structs, classes
 */

enum test_case_enum_t {
    TEST_CASE_XADD_SAME_VARIABLE = 0,
    TEST_CASE_XADD_SAME_CACHE_LINE,
    TEST_CASE_XADD_DIFF_CACHE_LINE,
    TEST_CASE_MAX
};

// This must match the enums for test_case_enum_t
static std::string test_case_names[] = {
    "fetch_add() on the same variable                            ",
    "fetch_add() on different variables but same cache line      ",
    "fetch_add() on different variables and different cache lines",
};

struct RunEntry {
    test_case_enum_t testCase;
    int numThreads;
    long opsPerSec;
};


class ScalableWFPO {

public:
    std::atomic<long>  sameVar;
    std::atomic<long>  sameCacheLines[128];
    std::atomic<long>  diffCacheLines[128*16];

    // Forward declaration
    class WorkerThread;
    WorkerThread **_workerThread = nullptr;

    class WorkerThread {
    public:
        std::atomic<bool> quit;
        std::atomic<long> aNumOps;
        long numOps;
        std::thread * const th = new std::thread(&WorkerThread::run, this);
        ScalableWFPO * const pbl;
        const test_case_enum_t testCase;
        const int tidx;

        WorkerThread(ScalableWFPO * const pbl, const test_case_enum_t testCase, const int tidx)
            : pbl(pbl),
              testCase(testCase),
              tidx(tidx) {
            quit.store(false);
        }

        ~WorkerThread() {
            delete th;
        }

        void run() {
            aNumOps.store(0);
            numOps = 0;
            while (!quit.load()) {
                switch(testCase) {
                case TEST_CASE_XADD_SAME_VARIABLE:
                    pbl->sameVar.fetch_add(1);
                    break;
                case TEST_CASE_XADD_SAME_CACHE_LINE:
                    pbl->sameCacheLines[tidx].fetch_add(1);
                    break;
                case TEST_CASE_XADD_DIFF_CACHE_LINE:
                    pbl->sameCacheLines[tidx*16].fetch_add(1);
                    break;
                }
                numOps++;
            }
            aNumOps.store(numOps);
        }
    };

public:
    ScalableWFPO(int numThreads, int numMilis);
    virtual ~ScalableWFPO();
    void singleTest(test_case_enum_t testCase);
    void addRun(test_case_enum_t testCase, int numThreads, long opsPerSec);
    void saveDB();

private:
    int numThreads;
    int numMilis;
};




/*
 * Global variables
 */
std::vector<RunEntry> _runs;


/*
 * Actual class for micro benchmarks
 */
ScalableWFPO::ScalableWFPO(int numThreads, int numMilis) {
	this->numThreads = numThreads;
	this->numMilis   = numMilis;
	_workerThread = new WorkerThread*[numThreads];
}


ScalableWFPO::~ScalableWFPO() {
    delete[] _workerThread;
}


/*
 *
 */
void ScalableWFPO::singleTest(test_case_enum_t testCase) {
	std::cout << "##### " << test_case_names[testCase] << " ##### \n";
    for(int i = 0; i < numThreads; i++ ) _workerThread[i] = new WorkerThread(this, testCase, i);

    std::chrono::milliseconds dura(numMilis);
    std::this_thread::sleep_for(dura);

    // Tell the worker threads to stop and join up with the other threads
    for (int i = 0; i < numThreads; i++) _workerThread[i]->quit.store(true);
    for (int i = 0; i < numThreads; i++) _workerThread[i]->th->join();

    // Measure the number of performed operations
    long long totalNumOps = 0;
    for (int i = 0; i < numThreads; i++) totalNumOps += _workerThread[i]->aNumOps.load();

    std::cout << "Total Ops/sec = " << (1000LL*totalNumOps/numMilis) << "   ";
    // Add the results to the database
    addRun(testCase, numThreads, (1000LL*totalNumOps/numMilis));

    for (int i = 0; i < numThreads; i++) delete _workerThread[i];
}




void ScalableWFPO::addRun(test_case_enum_t testCase, int numThreads, long opsPerSec) {
    RunEntry thisRun;
    thisRun.testCase = testCase;
    thisRun.numThreads = numThreads;
    thisRun.opsPerSec = opsPerSec;
    _runs.push_back(thisRun);
}


void ScalableWFPO::saveDB() {
    for (int testCase = 0; testCase < TEST_CASE_MAX; testCase++) {
        std::cout << test_case_names[testCase] << "\n";
        for (int i = 0; i < _runs.size(); i++) {
            RunEntry run = _runs[i];
            if (run.testCase != testCase) continue;
            std::cout << run.numThreads << ", " << run.opsPerSec << "\n";
        }
        std::cout << "\n";
    }
}




int main(void) {
    //int threadList[] = { 1, 2, 4, 8, 16, 24, 32 };
    int threadList[] = { 4 };
    test_case_enum_t testList[] = {
        TEST_CASE_XADD_SAME_VARIABLE, TEST_CASE_XADD_SAME_CACHE_LINE, TEST_CASE_XADD_DIFF_CACHE_LINE };
    int durationMiliseconds = 10000; // 10 seconds per test

    for (int ithread = 0; ithread < sizeof(threadList)/sizeof(test_case_enum_t); ithread++) {
        ScalableWFPO wfpo(threadList[ithread], durationMiliseconds);
        std::cout << "Number of threads = " << threadList[ithread] << "\n";

        for (int itest = 0; itest < sizeof(testList)/sizeof(test_case_enum_t); itest++) {
            wfpo.singleTest(testList[itest]);
            std::cout << "\n";
        }
    }

    std::cout << "\n Duration of tests is " << durationMiliseconds/1000 << " seconds \n";
    // We need an instance just to call saveDB
    ScalableWFPO wfpo(0, 0);
    wfpo.saveDB();

    return 0;
}
