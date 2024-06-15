## Exercise 1
**If you have not done so already, write down your estimate of the latency in
cycles of an add instruction: 0.1, 1, 10, 100**.

O(1) cy for an ADD that only touches registers, O(10) for an ADD that touches
L1 cache.


## Exercise 2
**Compile and run mystery1.cc bothg unoptimized `-O0` and optimized `-O2`.
Explain the differences.**

```
$ gcc -O0 mystery1.cc && ./a.out
1000000000 iterations, 3871752059 cycles, 3.87 cycles/iteration

$ gcc -O2 mystery1.cc && ./a.out
1000000000 iterations, 20 cycles, 0.00 cycles/iteration
```

With `-O2`, the compiler is clever realizes that the loop is dead code and
removes it completely.

```
0000000000001080 <main>:
    ...
    108f:       0f 31                   rdtsc
    1091:       48 89 c1                mov    rcx,rax
    1094:       48 c1 e2 20             shl    rdx,0x20
    1098:       48 09 d1                or     rcx,rdx
    109b:       0f 31                   rdtsc
    ...
```

With `-O0` however it still includes the loop.

```
0000000000001169 <main>:
    ...
16: int64_t startcy = GetCycles();
    119e:       e8 7a 00 00 00          call   121d <_Z9GetCyclesv>
    11a3:       48 89 45 e8             mov    QWORD PTR [rbp-0x18],rax

17: for (int i = 0; ...
    11a7:       c7 45 d0 00 00 00 00    mov    DWORD PTR [rbp-0x30],0x0
17: i < kIterations;
    11ae:       81 7d d0 ff c9 9a 3b    cmp    DWORD PTR [rbp-0x30],0x3b9ac9ff
    11b5:       7f 0f                   jg     11c6 <main+0x5d>

18: ... sum += incr; ...
    11b7:       8b 45 d4                mov    eax,DWORD PTR [rbp-0x2c]
    11ba:       48 98                   cdqe
    11bc:       48 01 45 d8             add    QWORD PTR [rbp-0x28],rax

17: ... ++i) {
    11c0:       83 45 d0 01             add    DWORD PTR [rbp-0x30],0x1
    11c4:       eb e8                   jmp    11ae <main+0x45>

20: int64_t elapsed = GetCycles() - startcy;
    11c6:       e8 52 00 00 00          call   121d <_Z9GetCyclesv>
    ...
```


## Exercise 3
**Uncomment the last `fprintf` in `mystery1.cc` and run again with both `-O0`
and `-O2`. Explain any changes or lack of change.**

```
$ gcc -O0 mystery1.cc && ./a.out
1000000000 iterations, 3808061895 cycles, 3.81 cycles/iteration
1718348582 38000000000

$ gcc -O2 mystery1.cc && ./a.out
1000000000 iterations, 20 cycles, 0.00 cycles/iteration
1718348589 45000000000
```

The `-O0` run takes 1% more cycles. This is not significant: likely due to OS
scheduling etc.

The `-O2` run still takes basically no time. Although the `fprintf` makes the
sum live, the compiler optimizes the sequence of adds into a multiply and puts
that outside the measurement boundaries.

```
0000000000001080 <main>:
    ...
    1090:       0f 31                   rdtsc
    1092:       48 89 c1                mov    rcx,rax
    1095:       48 c1 e2 20             shl    rdx,0x20
    1099:       48 09 d1                or     rcx,rdx
    109c:       0f 31                   rdtsc
    ...
    10e7:       4d 69 c0 00 ca 9a 3b    imul   r8,r8,0x3b9aca00
    ...
```


## Exercise 4
**Declare the variable `incr` in `mystery1.cc` as `volatile` and run again both
`-O0` and `-O2`. Explain any changes or lack of change.**

```
$ gcc -O0 mystery1.cc && ./a.out
1000000000 iterations, 3811241662 cycles, 3.81 cycles/iteration
1718348983 183000000000

$ gcc -O2 mystery1.cc && ./a.out
1000000000 iterations, 774809918 cycles, 0.77 cycles/iteration
1718348987 187000000000
```

`-O2` changes significantly: it has to run the loop now. The `volatile` forces
the compiler to consider `iter` as non-constant during the loop, and so it can't
optimize the adds in to a multiply.

## Exercise 5
**Make your own copy of `mystery1.cc` and modify it along the lines discussed
in this chapter to give a reasonable measurement of the latency in cycles of a
64-bit integer add. Write down your numeric answer.**

1 cycle.


## Exercise 6
**Experiment with the number of loop iterations in your copy of `mystery1.cc`:
1, 10, ... 1000000000. Explain why some values do not produce meaningful
results.**

Results:

```
$ gcc -O2 cpu.cc -lm && ./a.out -t 4.78 -b 3.6
tries per iter: 100
iters: 10^0     min: 23.900000  mean: 26.316556 CV: 0.059003
iters: 10^1     min: 2.390000   mean: 2.913144  CV: 0.536121
iters: 10^2     min: 1.354333   mean: 1.444622  CV: 0.081424
iters: 10^3     min: 1.064878   mean: 1.070667  CV: 0.005076
iters: 10^4     min: 1.034073   mean: 1.036832  CV: 0.002811
iters: 10^5     min: 1.031126   mean: 1.046152  CV: 0.096965
iters: 10^6     min: 1.030815   mean: 1.038224  CV: 0.014794
iters: 10^7     min: 1.012944   mean: 1.029100  CV: 0.014100
iters: 10^8     min: 1.007789   mean: 1.023758  CV: 0.008994
iters: 10^9     min: 1.017842   mean: 1.026602  CV: 0.012511
```

The first three powers (1, 10, 100) run few enough ADDs that the results are
heavily skewed by the overhead in running `rdtsc` (20-30 cycles). The rest all
get within 8% of the target in both mean and min. 10^8 does particularly well,
and 10^7 does about as well as 10^9 despite being 100x faster.
This probably comes down to the shorter loops running safely within the timer
interrupt cycle.

`10^7 cycles / (4.7 * 10^9 cycles/sec) = 2ms`, which is in the range of typical
timer cycles `1-10ms`.

The remaining variation might be due to hyperthreading? Not sure.

