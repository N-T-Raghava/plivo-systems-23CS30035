# Run Log

I tested each transport change separately before moving to the next design. The
goal was first to understand the failure mode, then improve reliability while
staying below the 2x bandwidth limit, and finally reduce the playout delay.

## Experiment 1 — Baseline

**Change:** None. The sender forwarded every frame once and the receiver
forwarded packets as they arrived.

**Why:** I started with the provided baseline to measure how packet loss and
network delay affect the stream without any recovery mechanism.

**Result:**
```
For 40ms delay Profile: 'A':

relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 88  (5.87%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 81, 'duplicated': 17}
================ SCORE ================
  frames               : 1500
  deadline misses      : 1054  (70.27%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

```
For 140ms delay Profile: 'A':

relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 34  (2.27%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 140ms delay Profile: 'B':

relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 81, 'duplicated': 17}
================ SCORE ================
  frames               : 1500
  deadline misses      : 81  (5.40%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

**Observation:** A lost UDP packet directly becomes a missing frame because
there is no redundancy or retransmission. Increasing the playout delay can help
with late packets, but it cannot recover packets that are actually dropped.

---

## Experiment 2 — Duplicate Transmission

**Change:** I sent every source frame twice instead of forwarding it only once.
The receiver was left unchanged.

**Why:** The baseline showed that a single dropped UDP packet permanently loses
the corresponding frame. Sending a second copy was the simplest proactive way
to reduce the probability of losing a frame without waiting for a
retransmission request.

**Result:**

| Profile | Delay (ms) | Misses | Miss Rate | Overhead | Result |
|---|---:|---:|---:|---:|---|
| A | 40 | 7 | 0.47% | 2.05x | INVALID |
| B | 40 | 742 | 49.47% | 2.05x | INVALID |
| A | 140 | 2 | 0.13% | 2.05x | INVALID |
| B | 140 | 6 | 0.40% | 2.05x | INVALID |

```
For 40ms delay Profile: 'A':

relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 74, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 167, 'duplicated': 30}
================ SCORE ================
  frames               : 1500
  deadline misses      : 742  (49.47%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

```
For 140ms delay Profile: 'A':

relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 74, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 2  (0.13%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 140ms delay Profile: 'B':

relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 167, 'duplicated': 30}
================ SCORE ================
  frames               : 1500
  deadline misses      : 6  (0.40%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

**Observation:** Sending every frame twice greatly reduced packet-loss failures
at a generous playout delay, but the measured bandwidth overhead was 2.05x,
which exceeded the 2.0x limit. Profile B at 40 ms also showed that duplication
does not solve packets arriving after their deadlines. This made full
duplication unsuitable and motivated using parity, where one recovery packet
can protect multiple source frames more efficiently.


---

## Experiment 3 — Pair XOR FEC

**Change:** I replaced full packet duplication with a small custom wire format.
Every original frame was still sent once, but I added one XOR parity packet for
each pair of consecutive frames. The receiver used the sequence information in
the packet tag to handle reordering and reconstruct a missing frame when the
other frame in its pair was available.

**Why:** Duplicate transmission improved reliability but exceeded the bandwidth
limit at 2.05x. Pair XOR parity allows one additional packet to protect two
source frames, reducing the amount of redundancy while still recovering
isolated packet losses.

**Result:**

| Profile | Delay (ms) | Misses | Miss Rate | Overhead | Result |
|---|---:|---:|---:|---:|---|
| A | 40 | 46 | 3.07% | 1.54x | INVALID |
| B | 40 | 971 | 64.73% | 1.54x | INVALID |
| A | 100 | 3 | 0.20% | 1.54x | VALID |
| B | 100 | 12 | 0.80% | 1.54x | VALID |
| A | 140 | 3 | 0.20% | 1.54x | VALID |
| B | 140 | 12 | 0.80% | 1.54x | VALID |

```
For 40ms delay Profile: 'A':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 46  (3.07%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 123, 'duplicated': 25}
================ SCORE ================
  frames               : 1500
  deadline misses      : 971  (64.73%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

```
For 140ms delay Profile: 'A':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 3  (0.20%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID

For 140ms delay Profile: 'B':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 123, 'duplicated': 25}
================ SCORE ================
  frames               : 1500
  deadline misses      : 12  (0.80%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID
```

```
For 100ms delay Profile: 'A':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 3  (0.20%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID

For 100ms delay Profile: 'B':

relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 123, 'duplicated': 25}
================ SCORE ================
  frames               : 1500
  deadline misses      : 12  (0.80%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID
```

**Observation:** Pair XOR FEC reduced the measured bandwidth overhead from 2.05x
with duplication to 1.54x while still producing valid runs at 100 ms and
140 ms on both profiles. The identical miss counts at 100 ms and 140 ms suggest
that the remaining misses at those delays were mainly loss patterns that a
single pair parity packet could not recover, rather than packets that simply
needed more time to arrive. In particular, if both source frames in a protected
pair are unavailable, the pair parity equation alone is not enough to recover
them. The remaining bandwidth budget therefore motivated adding a second parity
grouping to improve recovery without exceeding the 2x limit.