## Exercise 1
**In the first part of `mystery2.cc` that looks at cache line size timings,
what do you think the cache line size is and why?**

Results from my home gaming PC are below.

```
cycles per load:
        stride=16B      naive: 4.70     linear: 9.56    linear rolled: 7.29     scrambled: 24.99
        stride=32B      naive: 2.89     linear: 12.61   linear rolled: 11.20    scrambled: 36.44
        stride=64B      naive: 1.19     linear: 18.57   linear rolled: 22.53    scrambled: 74.10
        stride=128B     naive: 3.84     linear: 48.34   linear rolled: 71.38    scrambled: 196.14
        stride=256B     naive: 4.23     linear: 122.10  linear rolled: 139.70   scrambled: 247.53
        stride=512B     naive: 3.70     linear: 233.60  linear rolled: 153.60   scrambled: 230.92
        stride=1024B    naive: 6.16     linear: 195.50  linear rolled: 179.58   scrambled: 197.41
        stride=2048B    naive: 4.39     linear: 259.63  linear rolled: 213.97   scrambled: 234.60
        stride=4096B    naive: 7.37     linear: 339.29  linear rolled: 302.28   scrambled: 297.71
```

Based on `scrambled`, which we expect to be the most accurate, it would seem
that the line size is 128B. As originally modeled, we see an increase in
cy/load until a saturation point at `stride=128B: ~200 cy/load`.

[Agner's docs](https://agner.org/optimize/microarchitecture.pdf)
indicate that the true line size for this microarch (Coffee Lake) is 64B.
As suggested in the book, we're likely getting some distortion from N+1
fetching when testing 64B line size.


## Exercise 2
**In the first part of `mystery2.cc` that looks at cache line size timings,
explain a little about the three timings for a possible line size of 256B.**

I interpret the question as: explain the values we get from the naive, linear,
and scrambled algorithms with stride=256B.

First off, what do we expect? Well, we think the stride is smaller than 256B
and the cache is empty (trashed), so every fetch should be a miss all the way
down to main memory. From the table in ch 1 a main memory reference is
`O(100 ns) = O(300 cy)`.

<details>
  <summary>Explanations from before fixing the off-by-4x issue.</summary>

  The naive algorithm takes 6-7 cy per fetch. As the book notes, it makes no
  effort to defeat N+1 prefetching, so we might expect each fetch to take just
  4cy, and it's also vulnerable to parallel execution, so an average around 1cy +
  loop overhead might be reasonable. But consider that at 256B, every load comes
  from a different line. So 1 fetch per cycle when a fetch takes O(300 cy)
  implies that you'd need 300 fetches in flight at once. That's 6x the "50
  outstanding loads" that a core might have in flight, per the book! Accounting
  for that bottleneck we arrive at a figure of 6cy/fetch, which matches the
  observation.

  The linear algorithm takes ~28 cy/fetch. This one should avoid OOE and parallel
  fetches, since the fetches are dependent. But as the book notes, it may still
  be vulnerable to N+c prefetching because of the consistent access pattern.
  Perfect prefetching would lead me to predict 4cy/fetch, as above. So why do we
  see 7x slower? According to the Intel® 64 and IA-32 Architectures
  Optimization Reference Manual section E.3.4.2, the L1 strided prefetcher does
  not work for prefetch requests that cross 4KiB pages. So with a stride of 256B,
  we expect 15 fetches to be prefetched to L1 and the 16th (crossing the page
  boundary) to miss all the way to main memory. The mean access time then should be
  `(4cy * 15 + 300cy * 1) / 16 = 22.5cy`. That's pretty close! Certainly within
  the half order of magnitude that the 300cy is estimated to.

  The scrambled algorithm takes 60 cy/fetch. We expect that it defeats OOE and
  parallel fetches, and given a 256B stride on a 64B line size we should not see
  any trouble from N+1 prefetching. So where's the 5x savings coming from?
</details>

### Naive algorithm
The naive algorithm takes ~4cy/fetch, with 1 run in 10 taking as little as
1cy/fetch. As the book suggests, this code should be vulnerable to both N+c
prefetching (since the reads are predictably strided) and OOE (since the reads
are not dependent). N+c prefetching should make each read a 4cy L1 cache
reference, and OOE should allow higher throughput by parallelism.

That prediction bears out occasionally with the 1cy/fetch runs. The more common
mean of 4cy/fetch could indicate resource contention (?).

### Linear algorithm
The linear algorithm takes ~100cy/fetch. This one should avoid OOE and parallel
fetches, but still be vulnerable to N+c prefetching because of the consistent
access pattern. Naively, N+c prefetching should turn all the fetches into 4cy
L1 cache references. So what's happening?

Reading the
[Intel® 64 and IA-32 Architectures Optimization Reference Manual](https://www.intel.com/content/www/us/en/content-details/671488/intel-64-and-ia-32-architectures-optimization-reference-manual-volume-1.html)
section E.3.4.2 Data Prefetch to L1 caches gives some insight.
It says that "data prefetching works on loads only when ... prefetch request[s
are] within the page boundary of 4KiB". That would mean that with a stride of
256B only 15 of 16 reads are prefetched, and the last is a main memory access.
But it gets worse: strided access is tracked separately for each instruction,
so our effective stride is 1024B rather than 256B. That leaves us with one miss
out of every 4, for a mean access time of `(3*4cy + 1*400cy) / 4 = 100cy`.

The loop unrolling hurt us in this case!

Editor's note: this loop unrolling theory is not borne out by experiment.
Rolling the loop back up leads to slower measurements. Which is a real
surprise, because even with 4cy loads the loop overhead should be
non-dominating parallelizable.

### Scrambled algorithm
The scrambled algorithm takes ~300cy/fetch. We expect that the scrambled access
pattern should defeat N+c prefetching, and since our stride is 4x the true line
size of 64B we shouldn't see interference from N+1 prefetching. 235cy/fetch is
within a factor of two of our expectation (100ns = ~400cy/fetch).

Some factors that could cause that difference:
- the 100ns estimate was not measured on our system. is it too high?
- we might not be fully defeating DRAM shortcut access timing


## Exercise 3
**In the first part of `mystery2.cc` that looks at cache line size timings,
make a copy of the program, and in routing `MakeLongList()`, add a line after
`int extrabit = makelinear ? 0 : (1<<14);` that defeats the DRAM different-row
address patterns: `extrabit = 0;**

**Explain a little bit about the changes this produces in the scrambled line timings,
especially for possible line size of 128B.**

Results:
```
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 1.45     linear: 44.41   linear rolled: 56.53    scrambled: 194.71
                        scrambled: 194.71       scr no extrabit: 153.06
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 1.22     linear: 40.75   linear rolled: 64.83    scrambled: 199.09
                        scrambled: 199.09       scr no extrabit: 139.87
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 1.35     linear: 46.45   linear rolled: 64.23    scrambled: 200.28
                        scrambled: 200.28       scr no extrabit: 169.54
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 1.61     linear: 42.69   linear rolled: 66.51    scrambled: 203.70
                        scrambled: 203.70       scr no extrabit: 149.06
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 1.19     linear: 52.03   linear rolled: 70.30    scrambled: 202.00
                        scrambled: 202.00       scr no extrabit: 151.64
$ gcc -O2 memory.cc -lm && ./a.out -t 4.78 -b 3.6 | grep -A 1 'stride=128B'
        stride=128B     naive: 7.85     linear: 42.22   linear rolled: 67.16    scrambled: 208.79
                        scrambled: 208.79       scr no extrabit: 143.49
```

We see consistent results of a 25% = 50cy speedup when setting `extrabit=0`.
According to the book, a fresh DRAM access requires a precharge, row access,
and column access, and a read to the same row requires just a column access.
The book further says that each step costs 15ns, so the read to the same row
saves 30ns. 30ns is ~120cy on our test machine.

So to get a savings of 50cy on average, `extrabit=0` must make about 40% of
our accesses land in a row that's warmed up.


## Exercise 4
**In the second part that looks at total cache size `FindCacheSizes()`, what do
you think are the total sizes of L1, L2, and L3 caches?**

On the machine described in the book I think L1=32KiB, L2=256KiB, and L3=2MiB.

Editor's note: the L3 is in fact 3MiB.

On my machine, let's find out!

Results:
```
size=2KiB:
        dirty: 355.097569
        clean: 18.007986
        clean: 4.730208
        clean: 4.730208
size=4KiB:
        dirty: 271.115625
        clean: 9.377431
        clean: 4.315278
        clean: 4.315278
size=8KiB:
        dirty: 239.954340
        clean: 6.472917
        clean: 4.273785
        clean: 4.294531
size=16KiB:
        dirty: 230.027127
        clean: 6.120226
        clean: 5.082899
        clean: 5.010286
size=32KiB:
        dirty: 202.439431
        clean: 12.577582
        clean: 12.458290
        clean: 12.453103
size=64KiB:
        dirty: 188.845269
        clean: 12.689095
        clean: 12.437543
        clean: 12.406424
size=128KiB:
        dirty: 193.168327
        clean: 27.493039
        clean: 18.826177
        clean: 17.858870
size=256KiB:
        dirty: 169.347423
        clean: 41.476199
        clean: 31.846568
        clean: 31.832954
size=512KiB:
        dirty: 173.896748
        clean: 78.860794
        clean: 55.185440
        clean: 44.393679
size=1024KiB:
        dirty: 177.122670
        clean: 122.534990
        clean: 32.555678
        clean: 32.102010
size=2048KiB:
        dirty: 178.096461
        clean: 158.887445
        clean: 36.710089
        clean: 31.781655
size=4096KiB:
        dirty: 178.293087
        clean: 161.367668
        clean: 70.901583
        clean: 45.387122
size=8192KiB:
        dirty: 164.719792
        clean: 160.083065
        clean: 121.205814
        clean: 115.010464
size=16384KiB:
        dirty: 170.189967
        clean: 166.479712
        clean: 163.215011
        clean: 163.039972
size=32768KiB:
        dirty: 170.024684
        clean: 170.216642
        clean: 172.535388
        clean: 172.627492
```

The frequent spike in the first clean read is odd. Maybe caused by timer
interrupts? Unclear.

In general we see a jump at 32KiB from 4cy to 12cy,
an early rise at 128KiB leading to fully risen at 256KiB,
and a final rise at 8MiB to saturate at 16MiB.

So I would guess that my caches are L1=16KiB, L2=128KiB, L3=8MiB.

Agner and the web say L1=32KiB, L2=256KiB, L3=8MiB.
I must have been deceived by the "our code uses some extra memory" feature.


## Exercise 5
**What is your best estimate of the load-to-use time in cycles for each cache
level?**

- L1: 4cy.
- L2: 12cy.
- L3: ~32cy.


## Exercise 6
**To run on a CPU with a non-power of 2 cache size, such as an Intel i3 with
3MiB of L3 cache, how would you modify the code?**

I would either try half intervals or first run at powers of two, find the
sections where there's a rise, and then resample just those sections at much
higher density.


## Exercise 7
**In the second part that looks at cache sizes, explain the variation in cycle
counts within each cache level. The ones that barely fill a level are a bit
faster than the ones that completely fill a level. Why could that be?**

We assumed in our analysis that only the data that we're looping through ends
up in the cache, but this isn't true. Some other small bits of program data
end up in the cache, pushing out parts of the looped data and causing cache
misses. In addition, when the OS context switches, the cache may be partially
or completely wiped.


## Exercise 8
**Implement `FindCacheAssociativity()`. What is the associativity of each
level?**
