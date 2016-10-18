/*
 * main.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: pramalhe
 */
#include <thread>
#include <string>

#include "BenchmarkQ.hpp"


// g++ -std=c++14 main.cpp -I../include
int main(int argc, char *argv[]){

    if (argc == 1) {
        BenchmarkQ::allThroughputTests();
    } else {
        // The FK queue can run one run at a time, so lets do it from the cmdline:
        // First argument is number of threads
        /*
        const int nThreads = std::stoi(argv[1]);
        const int numPairs = 100*1000000LL;        // 100M for the paper
        const long long burstSize = 1000000LL;     // 1M for the paper
        const int numIters = 100;                  // Number of iterations of 1M enqueues/dequeues
        BenchmarkQ bench(nThreads);
        //std::cout << "\n----- (FK) Enq-Deq Benchmark   numThreads=" << nThreads << "   numPairs=" << numPairs/1000000LL << "M -----\n";
        //bench.enqDeqBenchmark<FKQueueC<BenchmarkQ::UserData>>(numPairs, 1);

        std::cout << "\n----- (FK) Burst Benchmark   numThreads=" << nThreads << "   burstSize=" << burstSize/1000LL << "K   numIters=" << numIters << " -----\n";
        bench.burstBenchmark<FKQueueC<BenchmarkQ::UserData>>(burstSize, numIters, 1);
        */
    }
    return 0;
}

