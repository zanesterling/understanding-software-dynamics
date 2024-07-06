// Routines to deal with simple spinlocks
// Copyright 2021 Richard L. Sites
// Quite possibly flawed

#include "spinlock.h"

#include <stdio.h>

// Acquire a spinlock, including a memory barrier to prevent hoisting loads
// Returns number of usec spent spinning
int32_t AcquireSpinlock(volatile char* lock) {
  int32_t safety_count = 0;
  // TODO: measure spinlock histogram
  // uint64_t startcy = GetCycles(); 
  char old_value;
  do {
    while (*lock != 0) {   // Spin without writing while someone else holds the lock
      ++safety_count;
      if (safety_count > 100000000) {
        fprintf(stderr, "safety_count exceeded. Grabbing lock\n");
        *lock = 0;
      }
    }
    // Try to get the lock
    old_value = __atomic_test_and_set(lock, __ATOMIC_ACQUIRE);
  } while (old_value != 0);
  // WE got the lock
  // uint64_t stopcy = GetCycles();
  // int64_t elapsed = stopcy - startcy;
  // int32_t usec = elapsed / kCyclesPerUsec; 
  // return usec;
  return 0;
}

// Release a spinlock, including a memory barrier to prevent sinking stores
void ReleaseSpinlock(volatile char* lock) {
  __atomic_clear(lock, __ATOMIC_RELEASE);
}

// Return floor of log base2 of x, i.e. the number of bits-1 needed to hold x
int32_t FloorLg(int32_t x) {
  int32_t lg = 0;
  int32_t local_x = x;
  if (local_x & 0xffff0000) {lg += 16; local_x >>= 16;}
  if (local_x & 0xff00) {lg += 8; local_x >>= 8;}
  if (local_x & 0xf0) {lg += 4; local_x >>= 4;}
  if (local_x & 0xc) {lg += 2; local_x >>= 2;}
  if (local_x & 0x2) {lg += 1; local_x >>= 1;}
  return lg;
}

// The constructor acquires the spinlock and the destructor releases it.
// Thus, just declaring one of these in a block makes the block run *only* when 
// holding the lock and then reliably release it at block exit 
SpinLock::SpinLock(LockAndHist* lockandhist) {
  lockandhist_ = lockandhist;
  int32_t usec = AcquireSpinlock(&lockandhist_->lock);
  ++lockandhist_->hist[FloorLg(usec)];
}

SpinLock::~SpinLock() {
  ReleaseSpinlock(&lockandhist_->lock);
}

