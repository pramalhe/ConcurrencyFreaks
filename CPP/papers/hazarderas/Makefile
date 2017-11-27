
MYDEPS = \
	MagedHarrisLinkedListNone.hpp \
	MagedHarrisLinkedListHE.hpp \
	MagedHarrisLinkedListHP.hpp \
	MagedHarrisLinkedListURCU.hpp \
	HazardEras.hpp \
	HazardPointers.hpp \
	URCUGraceVersion.hpp \
#	HazardErasWaitFree.hpp \
	MagedHarrisLinkedListHEWF.hpp \
	MagedHarrisLinkedListHPLB.hpp \
	MagedHarrisLinkedListHPLB2.hpp \
	MagedHarrisLinkedListHERange.hpp \
	HazardPointersLB.hpp \
	HazardPointersLB2.hpp \
	HazardErasRange.hpp \
	RingBuffer.hpp \
	

bench: $(MYDEPS) bench.cpp BenchmarkLists.hpp
	g++-7 -g -O3 -std=c++14 bench.cpp -o bench -lstdc++ -lpthread


bench-asan: $(MYDEPS) bench.cpp BenchmarkLists.hpp
	g++-7 -fuse-ld=gold -fsanitize=address -g -O3 -std=c++14 bench.cpp -o bench-asan -lstdc++ -lpthread


all: bench


	
