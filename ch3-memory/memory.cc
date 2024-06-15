// Want to measure:
// - cache line size
// - total size of each level of cache
// - associativity of each level of cache

#include <cstdio>
#include <stdint.h>
#include <time.h>
#include <x86intrin.h> // May need a different import for Windows.

#include "polynomial.h"

const size_t MAX_CACHE_SIZE_B = 40 * 1024 * 1024;
const size_t N_LOADS = 1024;

struct Pair {
  Pair* next;
  int64_t data;
};

// Always returns false, but the compiler doesn't know that.
// Used to make values live without a side effect.
bool NeverTrue() {
  return 0 == time(NULL);
}

void ClearCache(const uint8_t* ptr, const size_t byteSize) {
  uint64_t sum = 0;
  for (size_t i = 0; i < byteSize; ++i) {
    sum += ptr[i];
  }
  if (NeverTrue()) printf("%ld\n", sum);
}

Pair* MakeLinkedList(uint8_t* buf, size_t byteSize, uint64_t byteStride) {
  Pair* const rootptr = reinterpret_cast<Pair*>(buf);
  const size_t pairStride = byteStride / sizeof(Pair);

  Pair* curptr = rootptr;
  const size_t nPairs = byteSize / sizeof(Pair) / pairStride;
  for (int i = 0; i < nPairs; ++i) {
    curptr->next = rootptr + i*pairStride;
    curptr = curptr->next;
  }
  curptr->next = NULL;

  return rootptr;
}

int64_t NaiveTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  const Pair* pairptr = reinterpret_cast<Pair*>(ptr);
  int pairStride = byteStride / sizeof(Pair);
  int64_t sum = 0;

  ClearCache(ptr, byteSize);

  // Load N_LOADS items spaced by stride.
  // May have multiple loops outstanding; may have prefetching.
  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS / 4; i += 4) {
    sum += pairptr[0*pairStride].data;
    sum += pairptr[1*pairStride].data;
    sum += pairptr[2*pairStride].data;
    sum += pairptr[3*pairStride].data;
    pairptr += 4 * pairStride;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%ld\n", sum);
  return elapsed / N_LOADS; // Cycles per load.
}

// Defeats OOE. Still vulnerable to linear prefetching.
int64_t LinearTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLinkedList(ptr, byteSize, byteStride);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS / 4; i += 4) {
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return elapsed / N_LOADS; // Cycles per load.
}

// In a byte array, create a linked list of Pairs, spaced by the given stride.
// Pairs are generally allocated near the front of the array first and near the
// end of the array last. The list will have floor(bytesize / bytestride)
// elements. The last element's next field is NULL and all the data fields are
// zero.
//
// ptr must be aligned on a multiple of sizeof(void*), i.e. 8 for a 64-bit CPU
// bytestride must be a multiple of sizeof(void*), and  must be at least 16
//
// If makelinear is true, the list elements are at offsets 0, 1, 2, ... times
// stride. If makelinear is false, the list elements are in a scrambled order
// that is intended to defeat any cache prefetching hardware. See the POLY8
// discussion above.
//
// This routine is not intended to be particularly fast; it is called just once
//
// Returns a pointer to the first element of the list.
Pair* MakeLongList(uint8_t* ptr, int bytesize, int bytestride, bool makelinear) {
  // Make an array of 256 mixed-up offsets
  // 0, ff, e3, db, ... 7b, f6, f1
  int mixedup[256];
  // First element
  mixedup[0] = 0;
  // 255 more elements
  uint8_t x = POLYINIT8;
  for (int i = 1; i < 256; ++i) {
    mixedup[i] = x;
    x = POLYSHIFT8(x);
  }

  Pair* pairptr = reinterpret_cast<Pair*>(ptr);
  int element_count = bytesize / bytestride;
  // Make sure next element is in different DRAM row than current element
  int extrabit = makelinear ? 0 : (1 << 14);
  // Fill in N-1 elements, each pointing to the next one
  for (int i = 1; i < element_count; ++i) {
    // If not linear, there are mixed-up groups of 256 elements chained together
    int nextelement = makelinear ? i : (i & ~0xff) | mixedup[i & 0xff];
    Pair* nextptr = reinterpret_cast<Pair*>(ptr + ((nextelement * bytestride) ^ extrabit));
    pairptr->next = nextptr;
    pairptr->data = 0;
    pairptr = nextptr;
  }
  // Fill in Nth element
  pairptr->next = NULL;
  pairptr->data = 0;

  return reinterpret_cast<Pair*>(ptr);
}

// Defeats OOE. Still vulnerable to linear prefetching.
int64_t ScrambledTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLongList(ptr, byteSize, byteStride, /*makelinear=*/ false);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS / 4; i += 4) {
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return elapsed / N_LOADS; // Cycles per load.
}

uint16_t MeasureCacheLineSize() {
  // TODO: Is buf cache-line aligned?
  uint8_t* buf = (uint8_t*)malloc(MAX_CACHE_SIZE_B);

  printf("cycles per load:\n");
  for (
    uint64_t byteStride = sizeof(Pair);
    byteStride <= 4096;
    byteStride *= 2
  ) {
    int64_t naiveCyPerLoad = 0;
    int64_t linearCyPerLoad = 0;
    int64_t scrambledCyPerLoad = 0;
    for (int i = 0; i < 10; ++i) {
      naiveCyPerLoad += NaiveTiming(buf, MAX_CACHE_SIZE_B, byteStride);
      linearCyPerLoad += LinearTiming(buf, MAX_CACHE_SIZE_B, byteStride);
      scrambledCyPerLoad += ScrambledTiming(buf, MAX_CACHE_SIZE_B, byteStride);
    }
    naiveCyPerLoad /= 10;
    linearCyPerLoad /= 10;
    scrambledCyPerLoad /= 10;
    printf(
      "\tstride=%luB\tnaive: %ld\tlinear: %ld\tscrambled: %ld\n",
      byteStride, naiveCyPerLoad, linearCyPerLoad, scrambledCyPerLoad
    );
  }

  free(buf);
  return 0;
}

struct LevelSizes {
  uint64_t l1;
  uint64_t l2;
  uint64_t l3;
};
LevelSizes MeasureCacheLevelSizes() { return {0, 0, 0}; }

struct LevelAssocs {
  uint16_t l1;
  uint16_t l2;
  uint16_t l3;
};
LevelAssocs MeasureCacheLevelAssocs() { return {0, 0, 0}; }

int main(int argc, char** argv) {
  uint16_t line_size = MeasureCacheLineSize();
  printf("line size: %uB\n", line_size);
  printf("\n");

  LevelSizes level_sizes = MeasureCacheLevelSizes();
  printf("level sizes:\n");
  printf("\tL1: %luB\n", level_sizes.l1);
  printf("\tL2: %luB\n", level_sizes.l2);
  printf("\tL3: %luB\n", level_sizes.l3);
  printf("\n");

  LevelAssocs level_assocs = MeasureCacheLevelAssocs();
  printf("level assocs:\n");
  printf("\tL1: %uB\n", level_assocs.l1);
  printf("\tL2: %uB\n", level_assocs.l2);
  printf("\tL3: %uB\n", level_assocs.l3);
  printf("\n");

  return 0;
}
