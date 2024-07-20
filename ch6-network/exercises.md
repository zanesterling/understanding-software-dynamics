# Chapter 6

Consider this work:
1. Send 10 ping messages of 100KB each.
2. Send 10 writes of 1MB of random data for keys kkkkk, kkkkl, kkkkm, ...,
   kkkkt.
3. Send 10 matching reads of 1MB from the same 10 keys.
4. Finally, send a quit command.

Draw yourself a little sketch of what you expect to see in the RPC timings.

Now run the server4 program on one sample server and the client4 program on
another, sequentially sending commands for the previous sequences. Run the
dumplogfile4 program and makeself program against the first three client log
files and display the actual results.

You likely will find that the two servers’ wall-clock times differ by a few
milliseconds, which may be enough to make the HTML display look odd, if the
send time for a message is timestamped after the receipt time. We will look at
time alignment in the next chapter. In the meantime, you might consider
hand-editing the JSON files to adjust T2 and T3 to be between T1 and T4. This
is optional, but doing so will give you some insight about what your Chapter 7
program will need to do.

## Exercise 6.1
*How long, in milliseconds, did you estimate for the ping requests and their
response message transmissions? How long do they actually take? Briefly comment
on the difference.*

Assume ethernet is 1Gb/s ~= 100MB/s. A ping req/resp is ~100B, so transmission should be
100B / 100MB/s = 1us each direction.

Hmm, but it took 150-280us instead. What gives?

The logs show that the actual on-the-wire transmission time was right on the
money at my estimates: 1us. But 99.5% of the time is not spent on the wire. That
means it must be either:
- hanging out in kernel buffers waiting to be sent,
- hanging out in kernel buffers waiting to be processed,
- or interrupted by threading contention.

Hmm, but by default Linux seems to give you ~10ms before preemption. It would be
surprising if both client and server hit a preemption in a transaction that only
takes 0.5ms.

Could a clock shift explain it? No, I think a clock shift would produce a
lopsided output where one or the other of the two rabbit ears is clipped.
Instead we see mostly balanced ears.

I guess we have an SSH connection open to each of the servers. It's possible
that that's interfering with the network stack I suppose. But the load is so
light, contention would be surprising.

Maybe the network stack is waiting around to gather stuff up to make one big
packet because the payload is so small.

## Exercise 6.2
*How long, in milliseconds, did you estimate for the write requests and their
response message transmissions? How long do they actually take? Briefly comment
on the difference.*

Transmission of 1MB req is ~10ms.
Transmission of 100B resp is ~1us.

In reality req took 13ms, with ~8ms on the wire.
Resp took 240us, with ~1us on the wire. A surprise, but consistent with the ping
results.

## Exercise 6.3
*How long, in milliseconds, did you estimate for the read requests and their
response message transmissions? How long do they actually take? Briefly comment
on the difference.*

Transmission of 100B req is ~1us.
Transmission of 1MB resp is ~10ms.

In reality both req and resp took ~13ms.

Why is the req taking so long? We're just sending the key, right? It shouldn't
be that big.

I'm really not sure. Looking through the source code I don't see any reason why
we would be transmitting that much data.

