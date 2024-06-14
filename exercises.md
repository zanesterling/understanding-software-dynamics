## Exercise 1
**In the first part of `mystery2.cc` that looks at cache line size timings,
what do you think the cache line size is and why?**

Results from my home gaming PC are below.

```
cycles per load:
        stride=16B      naive: 1        linear: 1       scrambled: 6
        stride=32B      naive: 1        linear: 2       scrambled: 12
        stride=64B      naive: 3        linear: 6       scrambled: 19
        stride=128B     naive: 4        linear: 10      scrambled: 43
        stride=256B     naive: 5        linear: 27      scrambled: 54
        stride=512B     naive: 6        linear: 54      scrambled: 63
        stride=1024B    naive: 6        linear: 49      scrambled: 59
        stride=2048B    naive: 8        linear: 48      scrambled: 63
        stride=4096B    naive: 10       linear: 80      scrambled: 67
```

Note before moving on that my measurements differ from the book's by a factor
of four. This is intentional. I believe the book code inflates measurements for
the linear and scrambled tests. It measures the cycles taken when running 256
loop iterations, each containing 4 loads, then divides by 256 to get the
cy/loop. But assuming the loads are dependent, that's 4x the actual cy/load! My
code divides by a further 4 to get what I believe is a more accurate number.

Based on `scrambled`, which we expect to be the most accurate, it would seem
that the line size is either 128B or 256B. As originally modeled, we see an
increase in cy/load until a saturation point at `stride=256B: ~60 cy/load`.

As suggested in the book, we're likely getting some distortion from the
same-row DRAM access optimization, and maybe some from N+1 fetching.

[Agner's docs](https://agner.org/optimize/microarchitecture.pdf)
indicate that the true line size for this microarch (Coffee Lake) is 64B.
