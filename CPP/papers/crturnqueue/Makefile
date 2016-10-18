all: bench

MYDEPS = \
	include/HazardPointers.hpp \
	include/CRTurnQueue.hpp \
	include/MichaelScottQueue.hpp \


	
# For debugging generated code use: -S -fverbose-asm 
bench: $(MYDEPS) include/BenchmarkQ.hpp src/benchmark.cpp
	g++-5 -std=c++14 -Wall -g -O3 src/benchmark.cpp -I./include -o bench -lpthread

# This target builds with address sanitizer (and leak checker)
bench-asan: $(MYDEPS) include/BenchmarkQ.hpp src/benchmark.cpp
	g++-5 -std=c++14 -Wall -g -fsanitize=address src/benchmark.cpp -I./include -o bench-asan -lpthread

latency: $(MYDEPS) include/BenchmarkLatencyQ.hpp src/latency.cpp
	g++-5 -std=c++14 -Wall -g -O3 src/latency.cpp -I./include -o latency -lpthread

latency-asan: $(MYDEPS) include/BenchmarkLatencyQ.hpp src/latency.cpp
	g++-5 -std=c++14 -Wall -g -fsanitize=address -O3 src/latency.cpp -I./include -o latency-asan -lpthread



# Windows targets
bench.exe: $(MYDEPS) include/BenchmarkQ.hpp src/benchmark.cpp
	g++ -std=c++14 -Wall -g -O3 src/benchmark.cpp -I./include -o bench.exe

latency.exe: $(MYDEPS) include/BenchmarkLatencyQ.hpp src/latency.cpp
	g++ -std=c++14 -Wall -g -O3 src/latency.cpp -I./include -o latency.exe


