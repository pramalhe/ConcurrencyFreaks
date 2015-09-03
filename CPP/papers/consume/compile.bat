@set PATH=C:\MinGW\bin;%PATH%
@rem compile > a.txt 2>&1

@rem For std::shared_mutex
g++ -Wall -O3 -std=c++14 -I../../leftright -I../../readindicators -I../../rcu PerformanceBenchmarkConsume.cpp -o consume.exe -lstdc++ -lpthread

