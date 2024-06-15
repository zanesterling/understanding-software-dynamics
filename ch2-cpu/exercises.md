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


## Exercise 7
**Write down your order-of-magnitude estimates of the latency in cycles for
64-bit integer multiply and divide and double-precision floating-point add,
multiply, and divide.**

**You actually know these. For example, multiplies are likely to consume at
least one bit of multiplicand per cycle, so are unlikely to take more than 64
cycles plus a little startup time. Similarly, divides are likely to produce at
least one bit of quotient per cycle. Either may in fact process 2, 3, or 4 bits
per cycle.**

- imul: O(10-100) - 64b / 1-4b per cycle
- idiv: O(10-100) - 64b / 1-4b per cycle
- fadd: O(1) - 13b shift + iadd + max
- fmul: O(10) - 50b add + 14b mul + adjust
- fdiv: O(10) - 50b sub + 14b div + adjust


## Exercise 8
**Add code to your mystery1.cc to measure the latency of 64-bit integer
multiply and divide and double-precision add, multiply, and divide. Write down
the five numeric answers.**

**You might want to reduce the number of loop iterations by 10x or so, based on
what you learned in Exercise 2.6. For the floating-point calculations, keep
your data values away from the extremes of overflow/underflow, but also not
exactly 1.0 or 0.0.**

Estimates:

```
$ gcc -O2 cpu.cc -lm && ./a.out -t 4.78 -b 3.6
ADD:    1.036636 cycles
IMUL:   3.115324 cycles
DIV:    32.668203 cycles
FADD:   4.161362 cycles
FMUL:   4.149092 cycles
FDIV:   13.535031 cycles
```

Comparing to Agner's results, `FADD` is slow by 1 cycle, `FMUL` is fast by 1,
and `FDIV` is fast by 1.5.

## Exercise 9 (optional)
**Deliberately have your double-precision multiply and divide loops above drift
into overflow and underflow ranges for the data values (i.e., larger than
10**306 or less than 1/10**306 for IEEE double-precision), printing out the
observed latency perhaps every 10,000 iterations. If the cycle latencies
suddenly change, explain what is going on.**

Results:
```
$ gcc -O2 cpu.cc -lm && ./a.out -t 4.78 -b 3.6
Overflow:
    1.000e+00 -   1.636e+43: 4.052900 cy/op
    1.636e+43 -   2.676e+86: 4.038300 cy/op
    2.676e+86 -  4.377e+129: 4.037500 cy/op
   4.377e+129 -  7.161e+172: 4.038000 cy/op
   7.161e+172 -  1.171e+216: 4.038500 cy/op
   1.171e+216 -  1.916e+259: 4.038000 cy/op
   1.916e+259 -  3.134e+302: 4.038000 cy/op
   3.134e+302 -         inf: 4.038300 cy/op
          inf -         inf: 4.038300 cy/op
          inf -         inf: 4.038000 cy/op

   -1.000e+00 -  -1.636e+43: 4.038000 cy/op
   -1.636e+43 -  -2.676e+86: 4.040400 cy/op
   -2.676e+86 - -4.377e+129: 4.038500 cy/op
  -4.377e+129 - -7.161e+172: 4.037500 cy/op
  -7.161e+172 - -1.171e+216: 4.037500 cy/op
  -1.171e+216 - -1.916e+259: 4.037500 cy/op
  -1.916e+259 - -3.134e+302: 4.037500 cy/op
  -3.134e+302 -        -inf: 4.038300 cy/op
         -inf -        -inf: 4.038500 cy/op
         -inf -        -inf: 4.038300 cy/op

Underflow:
    1.000e+00 -   2.249e-44: 4.042800 cy/op
    2.249e-44 -   5.057e-88: 4.039800 cy/op
    5.057e-88 -  1.137e-131: 4.038500 cy/op
   1.137e-131 -  2.557e-175: 4.038300 cy/op
   2.557e-175 -  5.751e-219: 4.038300 cy/op
   5.751e-219 -  1.293e-262: 4.037200 cy/op
   1.293e-262 -  2.908e-306: 4.038300 cy/op
   2.908e-306 -  2.421e-322: 133.642900 cy/op
   2.421e-322 -  2.421e-322: 254.049000 cy/op
   2.421e-322 -  2.421e-322: 200.933400 cy/op

   -1.000e+00 -  -2.249e-44: 4.038000 cy/op
   -2.249e-44 -  -5.057e-88: 4.056800 cy/op
   -5.057e-88 - -1.137e-131: 4.037500 cy/op
  -1.137e-131 - -2.557e-175: 4.037700 cy/op
  -2.557e-175 - -5.751e-219: 4.142600 cy/op
  -5.751e-219 - -1.293e-262: 4.126200 cy/op
  -1.293e-262 - -2.908e-306: 4.142400 cy/op
  -2.908e-306 - -2.421e-322: 210.286200 cy/op
  -2.421e-322 - -2.421e-322: 174.418400 cy/op
  -2.421e-322 - -2.421e-322: 150.653900 cy/op
```

Overflow operations do not take longer than normal, but underflow takes FAR
longer: 100-200 extra cycles per operation!

According to the book, this extra work might be explained by these paths
requiring special handling in microcode rather than in hardware. I don't really
understand what about these edge case values makes them so hard to handle in
hardware.
