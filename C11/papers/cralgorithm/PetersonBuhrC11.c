// Peterson-Buhr in C11 with padding
#include <stdatomic.h>

enum Intent { DontWantIn, WantIn };

#define CACHE_LINE  64
#define PADRATIO    (CACHE_LINE/sizeof(atomic_int))
#define inv( c ) ((c) ^ 1)

typedef struct {
    atomic_int qarray[3*PADRATIO];
} TokenC11;

typedef struct {
    TokenC11 *ns;                                           // pointer to path node from leaf to root
    int       es;                                           // left/right opponent
} Tuple;

static Tuple    **states CALIGN;                           // handle N threads
static int       *levels CALIGN;                              // minimal level for binary tree
static TokenC11  *t CALIGN;


static inline void binary_prologue( int id, TokenC11 *t ) {
    int other = inv( id );
    atomic_int *q[2] = { &t->qarray[0], &t->qarray[PADRATIO] };
    atomic_int *turn = &t->qarray[2*PADRATIO];
    atomic_store_explicit(q[id], WantIn, memory_order_release);
    atomic_store(turn, id);                                 // RACE
    while ( atomic_load(q[other]) != DontWantIn && atomic_load_explicit(turn, memory_order_acquire) == id ) Pause(); // busy wait
} // binary_prologue

static inline void binary_epilogue( int id, TokenC11 *t ) {
    atomic_int *q[2] = { &t->qarray[0], &t->qarray[PADRATIO] };
    atomic_store_explicit(q[id], DontWantIn, memory_order_release);                  // exit protocol
} // binary_epilogue



static void *Worker( void *arg ) {
    TYPE id = (size_t)arg;
	uint64_t entry;
#ifdef FAST
	unsigned int cnt = 0, oid = id;
#endif // FAST

    int level = levels[id];
    Tuple *state = states[id];

    for ( int r = 0; r < RUNS; r += 1 ) {
        entry = 0;
        while ( atomic_load(&stop) == 0 ) {
            for ( int lv = 0; lv <= level; lv += 1 ) {      // entry protocol
                binary_prologue( state[lv].es, state[lv].ns );
            } // for

            CriticalSection( id );

            for ( int lv = level; lv >= 0; lv -= 1 ) {  // exit protocol, retract reverse order
                binary_epilogue( state[lv].es, state[lv].ns );
            } // for
#ifdef FAST
            id = startpoint( cnt );                     // different starting point each experiment
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
    // element 0 not used
    t = Allocator( N * sizeof(__typeof__(t[0])) );

    // states[id][s].es indicates the left or right contender at a match.
    // states[id][s].ns is the address of the structure that contains the match data.
    // s ranges from 0 to the tree level of a start point (leaf) in a minimal binary tree.
    // levels[id] is level of start point minus 1 so bi-directional tree traversal is uniform.

    states = Allocator( N * sizeof(__typeof__(states[0])) );
    levels = Allocator( N * sizeof(__typeof__(levels[0])) );
    levels[0] = -1;                                     // default for N=1
    for ( int id = 0; id < N; id += 1 ) {
        atomic_store(&t[id].qarray[0], DontWantIn);
        atomic_store(&t[id].qarray[PADRATIO], DontWantIn);
        atomic_store(&t[id].qarray[2*PADRATIO], 0);
        unsigned int start = N + id, level = Log2( start );
        states[id] = Allocator( level * sizeof(__typeof__(states[0][0])) );
        levels[id] = level - 1;
        for ( unsigned int s = 0; start > 1; start >>= 1, s += 1 ) {
            states[id][s].es = start & 1;
            states[id][s].ns = &t[start >> 1];
        } // for
    } // for
} // ctor

void dtor() {
    free( (void *)levels );
    free( (void *)states );
    free( (void *)t );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DNDEBUG -fno-reorder-functions -DPIN -DAlgorithm=CorreiaRamalhete Harness.c -lpthread -lm" //
// End: //
