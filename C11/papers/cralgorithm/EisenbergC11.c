// Murray A. Eisenberg and Michael R. McGuire}, Further Comments on Dijkstra's Concurrent Programming Control Problem,
// CACM, 1972, 15(11), p. 999
#include <stdatomic.h>
enum Intent { DontWantIn, WantIn, EnterCS };

static atomic_int *control CALIGN, HIGH CALIGN;

static void *Worker( void *arg ) {
	TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
		  L0: atomic_store(&control[id], WantIn);						// entry protocol
			// step 1, wait for threads with higher priority
		  L1: for ( int j = atomic_load(&HIGH); j != id; j = cycleUp( j, N ) )
				if ( atomic_load(&control[j]) != DontWantIn ) { Pause(); goto L1; } // restart search
		    atomic_store(&control[id], EnterCS);
			// step 2, check for any other thread finished step 1
			for ( int j = 0; j < N; j += 1 )
				if ( j != id && atomic_load(&control[j]) == EnterCS ) goto L0;
			if ( atomic_load(&control[HIGH]) != DontWantIn && atomic_load(&HIGH) != id ) goto L0;
			atomic_store(&HIGH, id);									// its now ok to enter
			CriticalSection( id );
			// look for any thread that wants in other than this thread
//			for ( int j = cycleUp( id + 1, N );; j = cycleUp( j, N ) ) // exit protocol
			for ( int j = cycleUp( atomic_load(&HIGH) + 1, N );; j = cycleUp( j, N ) ) // exit protocol
				if ( atomic_load(&control[j]) != DontWantIn ) { atomic_store(&HIGH, j); break; }
			atomic_store(&control[id], DontWantIn);
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
	control = Allocator( sizeof(__typeof__(control[0])) * N );
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		control[i] = ATOMIC_VAR_INIT(DontWantIn);
	} // for
	HIGH = ATOMIC_VAR_INIT(0);
} // ctor

void dtor() {
	free( (void *)control );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=Eisenberg Harness.c -lpthread -lm" //
// End: //
