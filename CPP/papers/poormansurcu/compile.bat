@set PATH=C:\MinGW\bin;%PATH%
@rem compile > a.txt 2>&1
@rem for Bullet Proof add -DURCU_BULLET_PROOF_LIB -lurcu-bp

g++ -Wall -O3 -std=c++14 PerformanceBenchmarkURCU.cpp -o urcu.exe -lstdc++ -lpthread

@rem for Linux/ppc
@rem g++-4.9 -Wall -O3 -std=c++14 PerformanceBenchmarkURCU.cpp -o urcu.exe -lstdc++ -lpthread -DURCU_BULLET_PROOF_LIB -lurcu-bp -I/root/userspace-rcu-master/  -L/root/userspace-rcu-master/.libs/
