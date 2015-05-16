// Leslie Lamport, A Fast Mutual Exclusion Algorithm, ACM Transactions on Computer Systems, 5(1), 1987, Fig. 2, p. 5
// N => do not want in, versus 0 in original paper, so "b" is dimensioned 0..N-1 rather than 1..N.
#include <stdatomic.h>
#include <stdbool.h>

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))
#define await( E ) while ( ! (E) ) Pause()

static atomic_int *b CALIGN;
static atomic_int x CALIGN, y CALIGN;
static TYPE PAD CALIGN __attribute__(( unused ));		// protect further false sharing


static void *Worker( void *arg ) {
	TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

	for ( int r = 0; r < RUNS; r += 1 ) {
		entry = 0;
		while ( atomic_load(&stop) == 0 ) {
		  start: atomic_store(&b[id*PADRATIO], true);							// entry protocol
		    atomic_store(&x, id);
			if ( FASTPATH( atomic_load(&y) != N ) ) {
			    atomic_store(&b[id*PADRATIO], false);
				await( atomic_load(&y) == N );
				goto start;
			} // if
			atomic_store(&y, id);
			if ( FASTPATH( atomic_load(&x) != id ) ) {
			    atomic_store(&b[id*PADRATIO], false);
				for ( int j = 0; j < N; j += 1 )
					await( ! atomic_load(&b[j*PADRATIO]) );
				if ( FASTPATH( atomic_load(&y) != id ) ) {
//					await( y == N );
					goto start;
				} // if
			} // if
			CriticalSection( id );
			atomic_store(&y, N);										// exit protocol
			atomic_store(&b[id*PADRATIO], false);
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
	b = Allocator( sizeof(__typeof__(b[0])) * N * PADRATIO);
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		b[i*PADRATIO] = ATOMIC_VAR_INIT(0);
	} // for
	y = ATOMIC_VAR_INIT(N);
} // ctor

void dtor() {
	free( (void *)b );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=LamportFast Harness.c -lpthread -lm" //
// End: //
