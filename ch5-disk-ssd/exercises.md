## Exercise 1
**What causes groups of about 150-250 disk blocks with time gaps between? About
what is the time between groups? What is causing this delay?**

Each group is one track on a disk. There is some physical space in between
tracks where no data is written. The time gaps between groups are ~1.5ms seeks
between the end of one track and the start of the next.

## Exercise 2
**Extra credit: If some groups are one block shorter than others, why?**

The physical track length might not be evenly divisible by 4KiB, putting the
roundoff in one or the other of the tracks. Alternatively there might be a
couple preserved blocks per cylinder for bad block detection. Or maybe some of
those "missing" blocks are actually bad blocks that have been skipped!

TODO: Exercises 3 to 6 on disk.

## Exercise 7
**Now run mystery3 on an SSD using /datassd/dserve. What is the seek time to
read the soonest-delivered block? What is the overall transfer rate start to
finish, in MiB/sec?**

Seek-time-to-first: `367us`.

Overall transfer rate:
- Last delivery: `897us`
- Num 4KiB blocks: `10,239`
- Total file size: `40.00MiB`
- Transfer rate: `40.00MiB / 897us = 46000 MiB/s`

