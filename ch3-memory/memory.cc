// Want to measure:
// - cache line size
// - total size of each level of cache
// - associativity of each level of cache

#include <cstdio>
#include <stdint.h>
#include <time.h>
#include <x86intrin.h> // May need a different import for Windows.
#include <stdlib.h>

#include "polynomial.h"
#include "../util/args.h"

const size_t MAX_CACHE_SIZE_B = 80 * 1024 * 1024;
const size_t N_LOADS = 256;
const size_t PAGE_SIZE = 4096;

Args args;

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

double NaiveTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  const Pair* pairptr = reinterpret_cast<Pair*>(ptr);
  int pairStride = byteStride / sizeof(Pair);
  int64_t sum = 0;

  ClearCache(ptr, byteSize);

  // Load N_LOADS items spaced by stride.
  // May have multiple loops outstanding; may have prefetching.
  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS; i += 4) {
    sum += pairptr[0 * pairStride].data;
    sum += pairptr[1 * pairStride].data;
    sum += pairptr[2 * pairStride].data;
    sum += pairptr[3 * pairStride].data;
    pairptr += 4 * pairStride;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%ld\n", sum);
  return args.clock_multiplier * elapsed / N_LOADS; // Cycles per load.
}

// Defeats OOE. Still vulnerable to linear prefetching.
double LinearTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLinkedList(ptr, byteSize, byteStride);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS; i += 4) {
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return args.clock_multiplier * elapsed / N_LOADS; // Cycles per load.
}

// Defeats OOE. Still vulnerable to linear prefetching.
double LinearRolledTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLinkedList(ptr, byteSize, byteStride);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS; ++i) {
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return args.clock_multiplier * elapsed / N_LOADS; // Cycles per load.
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
Pair* MakeLongList(uint8_t* ptr, int bytesize, int bytestride, bool makelinear, bool use_extrabit) {
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
  int extrabit = (makelinear || !use_extrabit) ? 0 : (1 << 14);
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
double ScrambledTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLongList(
    ptr, byteSize, byteStride, /*makelinear=*/ false, /*use_extrabit=*/true);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS; i += 4) {
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return args.clock_multiplier * elapsed / N_LOADS; // Cycles per load.
}

// Defeats OOE. Still vulnerable to linear prefetching.
double ScrambledNoExtrabitTiming(uint8_t* ptr, uint64_t byteSize, uint64_t byteStride) {
  Pair* pairptr = MakeLongList(
    ptr, byteSize, byteStride, /*makelinear=*/ false, /*use_extrabit=*/false);
  ClearCache(ptr, byteSize);

  // Unroll 4 times to attempt to reduce loop overhead in timing.
  uint64_t startcy = __rdtsc();
  for (int i = 0; i < N_LOADS; i += 4) {
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
    pairptr = pairptr->next;
  }
  uint64_t stopcy = __rdtsc();
  int64_t elapsed = stopcy - startcy; // Cycles

  if (NeverTrue()) printf("%p\n", pairptr);
  return args.clock_multiplier * elapsed / N_LOADS; // Cycles per load.
}

const uint64_t kMinStrideLg = 4;
const uint64_t kMaxStrideLg = 12;

void MeasureCacheLineSize() {
  // TODO: Is buf cache-line aligned?
  uint8_t* buf = (uint8_t*)aligned_alloc(1<<kMaxStrideLg, MAX_CACHE_SIZE_B);

  double naive_results[kMaxStrideLg - kMinStrideLg + 1];
  double linear_results[kMaxStrideLg - kMinStrideLg + 1];
  double linear_rolled_results[kMaxStrideLg - kMinStrideLg + 1];
  double scrambled_results[kMaxStrideLg - kMinStrideLg + 1];
  double scrambled_no_extrabit_results[kMaxStrideLg - kMinStrideLg + 1];

  printf("cycles per load:\n");
  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    naive_results[byteStrideLg - kMinStrideLg] = NaiveTiming(buf, MAX_CACHE_SIZE_B, byteStride);
  }
  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    linear_results[byteStrideLg - kMinStrideLg] = LinearTiming(buf, MAX_CACHE_SIZE_B, byteStride);
  }
  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    linear_rolled_results[byteStrideLg - kMinStrideLg] = LinearRolledTiming(buf, MAX_CACHE_SIZE_B, byteStride);
  }
  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    scrambled_results[byteStrideLg - kMinStrideLg] = ScrambledTiming(buf, MAX_CACHE_SIZE_B, byteStride);
  }
  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    scrambled_no_extrabit_results[byteStrideLg - kMinStrideLg] = ScrambledNoExtrabitTiming(buf, MAX_CACHE_SIZE_B, byteStride);
  }

  for (uint64_t byteStrideLg = kMinStrideLg; byteStrideLg <= kMaxStrideLg; ++byteStrideLg) {
    uint64_t byteStride = 1 << byteStrideLg;
    size_t i = byteStrideLg - kMinStrideLg;
    double naiveCyPerLoad = naive_results[i];
    double linearCyPerLoad = linear_results[i];
    double linearRolledCyPerLoad = linear_rolled_results[i];
    double scrambledCyPerLoad = scrambled_results[i];
    double scrambledNoExtrabitCyPerLoad = scrambled_no_extrabit_results[i];
    printf(
      "\tstride=%luB\tnaive: %.2f\tlinear: %.2f\tlinear rolled: %.2f\tscrambled: %.2f\n"
      "\t            \tscrambled: %.2f\tscr no extrabit: %.2f\n",
      byteStride, naiveCyPerLoad, linearCyPerLoad, linearRolledCyPerLoad, scrambledCyPerLoad,
      scrambledCyPerLoad, scrambledNoExtrabitCyPerLoad
    );
  }

  free(buf);
}

double ScrambledReads(const Pair* pair_ptr, uint64_t n_reads) {
  int64_t start_cy = __rdtsc();
  for (size_t i = 0; i < n_reads; i += 4) {
    pair_ptr = pair_ptr->next;
    pair_ptr = pair_ptr->next;
    pair_ptr = pair_ptr->next;
    pair_ptr = pair_ptr->next;
  }
  int64_t stop_cy = __rdtsc();
  if (NeverTrue()) fprintf(stdout, "%p\n", pair_ptr);
  return args.clock_multiplier * (stop_cy - start_cy) / n_reads;
}

struct SizeMeasurements {
  double dirty_fetch_cy;
  double clean_fetch_1_cy;
  double clean_fetch_2_cy;
  double clean_fetch_3_cy;
};
SizeMeasurements MeasureSize(
  const uint16_t line_size_bytes,
  const Pair* const root_pair,
  const size_t buf_size,
  const uint64_t cache_size_kb
) {
  ClearCache((uint8_t*)root_pair, buf_size);

  SizeMeasurements out;
  size_t lines_to_read = cache_size_kb * 1024 / line_size_bytes;
  out.dirty_fetch_cy = ScrambledReads(root_pair, lines_to_read);
  out.clean_fetch_1_cy = ScrambledReads(root_pair, lines_to_read);
  out.clean_fetch_2_cy = ScrambledReads(root_pair, lines_to_read);
  out.clean_fetch_3_cy = ScrambledReads(root_pair, lines_to_read);
  return out;
}

int minCacheSizeKbLg = 1;
int maxCacheSizeKbLg = 15;
void MeasureCacheLevelSizes(uint16_t line_size_bytes) {
  uint8_t* const buf = (uint8_t*)aligned_alloc(PAGE_SIZE, MAX_CACHE_SIZE_B);
  int byte_stride = line_size_bytes * 2; // Defeat N+1.
  bool makelinear = false;
  bool use_extrabit = true;
  Pair* pair_ptr = MakeLongList(buf, MAX_CACHE_SIZE_B, line_size_bytes*2, makelinear, use_extrabit);

  for (int lg = minCacheSizeKbLg; lg <= maxCacheSizeKbLg; ++lg) {
    const uint64_t cache_size_kb = 1ul << lg;
    const auto out = MeasureSize(line_size_bytes, pair_ptr, MAX_CACHE_SIZE_B, cache_size_kb);

    fprintf(
      stdout,
      "size=%luKiB:\n"
      "\tdirty: %f\n"
      "\tclean: %f\n"
      "\tclean: %f\n"
      "\tclean: %f\n",
      cache_size_kb,
      out.dirty_fetch_cy,
      out.clean_fetch_1_cy,
      out.clean_fetch_2_cy,
      out.clean_fetch_3_cy
    );
  }
}

void MeasureCacheLevelAssocs() {}

int main(int argc, char** argv) {
  args = ParseArgs(argc, argv);

  MeasureCacheLineSize();
  printf("\n");

  uint16_t line_size = 64; // Measured in the previous section, confirmed with Agner.
  MeasureCacheLevelSizes(line_size);
  printf("\n");

  MeasureCacheLevelAssocs();
  printf("\n");

  return 0;
}
