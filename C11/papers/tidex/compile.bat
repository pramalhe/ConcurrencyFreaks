@rem You may need -D_XOPEN_SOURCE=600 
@set PATH=C:\MinGW\bin;%PATH%
gcc -O3 --std=c11 -std=gnu11 benchmark.c ticket_mutex.c tidex_mutex.c -lpthread -o benchmark
