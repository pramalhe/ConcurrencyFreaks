
MYDEPS = \
	URCUGraceVersion.hpp \
	URCUGraceVersionSyncScale.hpp \
	URCUTwoPhase.hpp \
	RIAtomicCounter.hpp \
	RIAtomicCounterArray.hpp \
	RIEntryPerThread.hpp \
	

URCU_PATH = /mnt/c/Users/andreia/workspace/userspace-rcu
	

# Alternative working compiler is gcc.c4.9.3-p0.linux
urcu: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++-6 -g -O3 -std=c++14 urcu.cpp -o urcu -lstdc++ -lpthread


urcu-asan: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++-6 -fsanitize=address -g -O3 -std=c++14 urcu.cpp -o urcu-asan -lstdc++ -lpthread

# run with LD_LIBRARY_PATH=. ./urcu-bp
urcu-bp: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++-6 -g -O3 -std=c++14 urcu.cpp -o urcu-bp -lstdc++ -lpthread -DURCU_BULLET_PROOF_LIB -lurcu-bp -I$(URCU_PATH)/src -I$(URCU_PATH)/include -L$(URCU_PATH)/src/.libs/

# run with LD_LIBRARY_PATH=. ./urcu-mb
urcu-mb: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++-6 -g -O3 -std=c++14 -DURCU_MB urcu.cpp -o urcu-mb  -lurcu-mb -lstdc++ -lpthread  -I/home/vagrant/userspace-rcu-master/src -I/home/vagrant/userspace-rcu-master/include -L/home/vagrant/userspace-rcu-master/src/.libs/

urcu-linux: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++-6 -g -O3 -std=c++14 -DLINUX_URCU urcu.cpp -o urcu-linux  -lurcu -lstdc++ -lpthread  -I$(URCU_PATH)/src -I$(URCU_PATH)/include -L$(URCU_PATH)/src/.libs/

# TODO: enable -Wall
urcu.exe: $(MYDEPS) urcu.cpp BenchmarkURCU.hpp
	g++ -g -O3 -std=c++14 lists.cpp -o urcu.exe -lstdc++ -lpthread

stress: $(MYDEPS) stress.cpp StressTestURCU.hpp
	g++-6 -fsanitize=address -g -O3 -std=c++14 stress.cpp -o stress -lstdc++ -lpthread

all: urcu


	
