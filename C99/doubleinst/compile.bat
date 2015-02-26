@rem Add this if you want to try the pthread implementation of C-RW-WP  ../locksc99/crwwp_pthread.c crwwp_linkedlist.c 
@set PATH=C:\MinGW\bin;%PATH%
gcc -Wall -O3 -I../locksc99 benchmark_al.c rw_arraylist.c rw_linkedlist.c di_arraylist.c di_linkedlist.c -lpthread -o benchmark
