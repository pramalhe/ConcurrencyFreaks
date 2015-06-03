// Boleslaw K. Szymanski. A simple solution to Lamport's concurrent programming problem with linear wait.
// Proceedings of the 2nd International Conference on Supercomputing, 1988, Figure 2, Page 624.
// Waiting after CS can be moved before it.
#include <stdatomic.h>
static atomic_int *flag CALIGN;

#define await( E ) while ( ! (E) ) Pause()

static void *Worker( void *arg ) {
	TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	int j;

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
			atomic_store(&flag[id], 1);
			for ( j = 0; j < N; j += 1 )				// wait until doors open
				await( atomic_load(&flag[j]) < 3 );
			atomic_store(&flag[id], 3);								// close door 1
			for ( j = 0; j < N; j += 1 )				// check for 
				if ( atomic_load(&flag[j]) == 1 ) {					//   others in group ?
					atomic_store(&flag[id], 2);						// enter waiting room
				  L: for ( int k = 0; k < N; k += 1 )	// wait for
						if ( atomic_load(&flag[k]) == 4 ) goto fini;	//   door 2 to open
					goto L;
				  fini: ;
				} // if
			atomic_store(&flag[id], 4);								// open door 2

			for ( j = 0; j < id; j += 1 )				// service threads in priority order
				await( atomic_load(&flag[j]) < 2 );
			CriticalSection( id );
			for ( j = id + 1; j < N; j += 1 )			// wait for all threads in waiting room
				await( atomic_load_explicit(&flag[j], memory_order_acquire) < 2 ||
				       atomic_load_explicit(&flag[j], memory_order_acquire) > 3 );	//    to pass through door 2
			atomic_store_explicit(&flag[id], 0, memory_order_release);
#ifdef FAST
			id = startpoint( cnt );						// different starting point each experiment
			cnt = cycleUp( cnt, NoStartPoints );
#endif // FAST
			entry += 1;
		} // while
#ifdef FAST
		id = oid;
#endif // FAST
		entries[r][id] = entry;
        atomic_fetch_add( &Arrived, 1 );
        while ( atomic_load(&stop) != 0 ) Pause();
        atomic_fetch_add( &Arrived, -1 );
	} // for
	return NULL;
} // Worker

void ctor() {
    flag = Allocator( sizeof(atomic_int) * N );
    for ( int i = 0; i < N; i += 1 ) {                  // initialize shared data
        flag[i] = ATOMIC_VAR_INIT(0);
    } // for

} // ctor

void dtor() {
    free( flag );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc-4.9 -std=c11 -Wall -O3 -DNDEBUG -DPIN -DAlgorithm=SzymanskiC11 Harness.c -lpthread -lm" //
// End: //
