/*
 * main.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: pramalhe
 */
#include <thread>

#include "BenchmarkURCU.hpp"



// g++ -std=c++14 main.cpp -I../include
int main(void) {
    BenchmarkURCU::allThroughputTests();
    return 0;
}

