#ifndef _PERSISTENT_FENCES_
#define _PERSISTENT_FENCES_

/*
 * The naming for these macros and respective operations were taken from the excellent
 * "Preserving Happens-before in Persistent Memory" by Izraelevitz, Mendes, and Scott
 * https://www.cs.rochester.edu/u/jhi1/papers/2016-spaa-transform
 *
 * We have five different definitions of pwb/pfence/psync:
 * - Emulated: We introduce a delay on stores, like Mnemosyne does
 * - Nothing: only works with process restart persistency, i.e. process failures, but not system failure
 * - Define pwb as clflush (Broadwell cpus)
 * - Define pwb as clflushopt (most x86 cpus)
 * - Define pwb as clwb (only very recent cpus have this instruction)
 */

/*
 * We copied the methods from Menmosyne:
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.221.5462&rep=rep1&type=pdf
 */
static inline unsigned long long asm_rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

// Change this depending on the clock cycle of your cpu. For Cervino it's 2100, for my laptop it's 2712.
#define EMULATED_CPUFREQ  2100

#define NS2CYCLE(__ns) ((__ns) * EMULATED_CPUFREQ / 1000)

static inline void emulate_latency_ns(int ns) {
    uint64_t stop;
    uint64_t start = asm_rdtsc();
    uint64_t cycles = NS2CYCLE(ns);
    do {
        /* RDTSC doesn't necessarily wait for previous instructions to complete
         * so a serializing instruction is usually used to ensure previous
         * instructions have completed. However, in our case this is a desirable
         * property since we want to overlap the latency we emulate with the
         * actual latency of the emulated instruction.
         */
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}


/*
 * We use the settings on the delays for emulation from the NVMOVE paper:
 * http://www.cs.utexas.edu/~vijay/papers/inflow16-nvmove.pdf
 */
#ifdef PWB_IS_STT
  /* Delays for emulating STT in DRAM */
  #define PWB(addr)              emulate_latency_ns(140)
  #define PFENCE()               emulate_latency_ns(200)
  #define PSYNC()                emulate_latency_ns(200)
#elif PWB_IS_PCM
  /* Delays for emulating PCM in DRAM */
  #define PWB(addr)              emulate_latency_ns(340)
  #define PFENCE()               emulate_latency_ns(500)
  #define PSYNC()                emulate_latency_ns(500)
#elif PWB_IS_CLFLUSH
  /*
   * More info at http://elixir.free-electrons.com/linux/latest/source/arch/x86/include/asm/special_insns.h#L213
   * Intel programming manual at https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf
   * Use these for Broadwell CPUs (cervino server)
   */
  #define PWB(addr)              asm volatile("clflush (%0)" :: "r" (addr) : "memory")                      // Broadwell only works with this.
  #define PFENCE()               {}                                                                         // No ordering fences needed for CLFLUSH (section 7.4.6 of Intel manual)
  #define PSYNC()                {}                                                                         // For durability it's not obvious, but CLFLUSH seems to be enough, and PMDK uses the same approach
#elif PWB_IS_CLWB
  /* Use this for CPUs that support clwb, such as the SkyLake SP series (c5 compute intensive instances in AWS are an example of it) */
  #define PWB(addr)              asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(addr)))  // clwb() only for Ice Lake onwards
  #define PFENCE()               asm volatile("sfence" : : : "memory")
  #define PSYNC()                asm volatile("sfence" : : : "memory")
#elif PWB_IS_NOP
  /* pwbs are not needed for shared memory persistency (i.e. persistency across process failure) */
  #define PWB(addr)              {}
  #define PFENCE()               asm volatile("sfence" : : : "memory") // TODO: replace the stores on 'state' with store-releases and these won't be needed
  #define PSYNC()                asm volatile("sfence" : : : "memory")
#elif PWB_IS_CLFLUSHOPT
  /* Use this for CPUs that support clflushopt, which is most recent x86 */
  #define PWB(addr)              asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)))    // clflushopt (Kaby Lake)
  #define PFENCE()               asm volatile("sfence" : : : "memory")
  #define PSYNC()                asm volatile("sfence" : : : "memory")
#else
#error "You must define what PWB is. Choose PWB_IS_CLFLUSHOPT if you don't know what your CPU is capable of"
#endif

// Flush each cache line in a range
// TODO: fix cache alignment
inline static void flushFromTo(void* from, void* to) noexcept {
    const int cache_line_size = 64;
    uint8_t* ptr = (uint8_t*)from;
    for (; ptr < (uint8_t*)to; ptr += cache_line_size) PWB(ptr);
}

// TODO: Implement fences for ARM


#endif
