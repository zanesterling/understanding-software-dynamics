#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Sizes determined in exercises for Ch 3.
// Totally machine dependent!
// Change these if you're on a different machine.
const size_t L1_SIZE_BYTES = 32  * 1024;
const size_t L2_SIZE_BYTES = 256 * 1024;
const size_t L3_SIZE_BYTES = 16  * 1024 * 1024;

bool NeverTrue() { return time(NULL) == 0; }

void CleanCache() {
  uint8_t* buf = (uint8_t*)malloc(L3_SIZE_BYTES);
  uint64_t sum = 0;
  for (size_t i = 0; i < L3_SIZE_BYTES / 8; ++i) {
    sum += buf[i];
  }
  if (NeverTrue()) fprintf(stdout, "%lu\n", sum);
  free(buf);
}
