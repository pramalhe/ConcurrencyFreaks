Data structures and benchmarks using Double Instance Locking.
This technique does NOT use any atomics or anything from the memory model, 
which means that all this code compiles in a C99 compiler, such as GCC
(with pretty much any old version).

