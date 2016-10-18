/*
 * main.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: pramalhe
 */
#include <thread>

#include "StressTestQueues.hpp"


std::atomic<bool> StressTestQueues::errorFound;
// These variables are used only by the BurstConsumerThread instances
std::atomic<long> StressTestQueues::burstRound;
std::atomic<bool> StressTestQueues::burstQueueIsEmpty;
std::atomic<long> StressTestQueues::burstWaitingThreads;

// g++ -std=c++14 main.cpp -I../include
int main(void) {
    StressTestQueues::allStressTests();
    //StressTestQueues::nightTrain();

    return 0;
}

