// Routines to deal with simple spinlocks
// Copyright 2021 Richard L. Sites
// Quite possibly flawed

#include <stdint.h>

struct LockAndHist {
  volatile char lock;	// One-byte spinlock
  char pad[7];		// align the histogram
  uint32_t hist[32];	// histogram of spin time, in buckets of floor(lg(usec))
};

// The constructor for this acquires the spinlock and the destructor releases it.
// Thus, just declaring one of these in a block makes the block run *only* when 
// holding the lock and then reliably release it at block exit 
class SpinLock {
public:
  SpinLock(LockAndHist* lockandhist);
  ~SpinLock();

  LockAndHist* lockandhist_;
};


// Acquire a spinlock, including a memory barrier to prevent hoisting loads
// Returns number of usec spent spinning
int32_t AcquireSpinlock(volatile char* lock);

// Release a spinlock, including a memory barrier to prevent sinking stores
void ReleaseSpinlock(volatile char* lock);


