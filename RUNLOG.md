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

endpoints done
relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 88  (5.87%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 34  (2.27%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 140ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 74, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 492000, 'down_bytes': 0, 'up_pkts': 3000, 'down_pkts': 0, 'dropped': 74, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 2  (0.13%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 2.05x   [cap 2.00x]   (up 492000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 140ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 46  (3.07%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)

For 40ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 3  (0.20%)   [cap 1.00%]
  playout delay        : 140 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID

For 140ms delay Profile: 'B':

endpoints done
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

endpoints done
relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 3  (0.20%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID

For 100ms delay Profile: 'B':

endpoints done
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

---

## Experiment 4 — Pair and Triple XOR FEC

**Change:** I kept the pair XOR recovery from the previous experiment and added
a second parity stream over groups of three consecutive frames. The two parity
groupings overlap differently, so a frame that cannot be recovered from its
pair may still become recoverable from its triple. The receiver also performs
iterative recovery, allowing a newly reconstructed frame to unlock another
parity equation.

**Why:** Pair-only FEC reduced the bandwidth overhead to 1.54x and was valid at
100 ms, but Profile B still had 12 misses (0.80%) even when the delay was
increased to 140 ms. Since additional delay did not reduce those misses, I used
part of the remaining bandwidth budget for a second, differently grouped parity
stream rather than simply increasing the playout delay.

**Result:**

| Profile | Delay (ms) | Misses | Miss Rate | Overhead | Result |
|---|---:|---:|---:|---:|---|
| A | 120 | 0 | 0.00% | 1.88x | VALID |
| A | 115 | 0 | 0.00% | 1.88x | VALID |
| A | 110 | 0 | 0.00% | 1.88x | VALID |
| A | 105 | 0 | 0.00% | 1.88x | VALID |
| A | 100 | 0 | 0.00% | 1.88x | VALID |
| A | 95 | 0 | 0.00% | 1.88x | VALID |
| A | 90 | 0 | 0.00% | 1.88x | VALID |
| A | 85 | 0 | 0.00% | 1.88x | VALID |
| A | 80 | 0 | 0.00% | 1.88x | VALID |
| B | 120 | 7 | 0.47% | 1.88x | VALID |
| B | 115 | 7 | 0.47% | 1.88x | VALID |
| B | 110 | 7 | 0.47% | 1.88x | VALID |
| B | 105 | 7 | 0.47% | 1.88x | VALID |
| B | 100 | 7 | 0.47% | 1.88x | VALID |
| B | 95 | 11 | 0.73% | 1.88x | VALID |
| B | 90 | 15 | 1.00% | 1.88x | VALID |
| B | 85 | 21 | 1.40% | 1.88x | INVALID |
| B | 80 | 25 | 1.67% | 1.88x | INVALID |

```
========== PROFILE A: 120ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 120 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 115ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 115 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 110ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 110 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 105ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 105 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 100ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 95ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 95 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 90ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 90 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 85ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 85 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE A: 80ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 65, 'duplicated': 19}
================ SCORE ================
  frames               : 1500
  deadline misses      : 0  (0.00%)   [cap 1.00%]
  playout delay        : 80 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 120ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 120 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 115ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 115 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 110ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 110 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 105ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 105 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 100ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 7  (0.47%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 95ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 11  (0.73%)   [cap 1.00%]
  playout delay        : 95 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 90ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 15  (1.00%)   [cap 1.00%]
  playout delay        : 90 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : VALID
========== PROFILE B: 85ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 21  (1.40%)   [cap 1.00%]
  playout delay        : 85 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
========== PROFILE B: 80ms ==========
endpoints done
relay done: {'up_bytes': 451000, 'down_bytes': 0, 'up_pkts': 2750, 'down_pkts': 0, 'dropped': 152, 'duplicated': 29}
================ SCORE ================
  frames               : 1500
  deadline misses      : 25  (1.67%)   [cap 1.00%]
  playout delay        : 80 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.88x   [cap 2.00x]   (up 451000B, feedback 0B)
  RESULT               : INVALID
  (reduce misses under 1% and overhead under 2x, THEN minimize delay)
```

**Observation:** Adding triple parity increased the measured overhead from 1.54x
to 1.88x, while remaining below the 2.0x limit. The additional recovery
equations reduced Profile A from 3 misses at 100 ms with pair-only FEC to zero,
and reduced Profile B from 12 misses to 7 at the same delay.

Profile A remained valid with zero misses throughout the tested range down to
80 ms. Profile B was the limiting profile: 100 ms produced 0.47% misses, 95 ms
produced 0.73%, and 90 ms reached exactly the 1.00% validity limit. At 85 ms the
miss rate increased to 1.40%, making the run invalid.

The lowest demonstrated valid playout delay on both provided profiles was
therefore 90 ms. However, 90 ms sits exactly on the allowed miss-rate boundary,
while 95 ms and 100 ms provide progressively more margin. The final design
therefore exposes a clear latency-versus-robustness trade-off rather than relying
on a single test point.

---

## Final Comparison

| Design | Overhead | Profile A @ 100 ms | Profile B @ 100 ms | Outcome |
|---|---:|---:|---:|---|
| Duplicate transmission | 2.05x | — | — | Invalid: bandwidth exceeded |
| Pair XOR FEC | 1.54x | 0.20% | 0.80% | Valid |
| Pair + Triple XOR FEC | 1.88x | 0.00% | 0.47% | Valid |

The experiments showed that simply duplicating every packet was reliable at
large delays but exceeded the bandwidth limit. Pair XOR FEC was much more
bandwidth-efficient and produced valid results, but some loss patterns remained
unrecoverable. Adding a second, differently grouped triple parity stream used
the remaining bandwidth budget to improve recovery and reduced the miss rate on
both provided profiles.

The final implementation sends each source frame once, one XOR parity packet
for every pair of frames, and one XOR parity packet for every group of three
frames. The receiver uses explicit sequence information to tolerate reordering
and duplication, reconstructs a frame whenever exactly one member of a parity
group is missing, and immediately reuses recovered frames in further recovery
attempts.

The lowest demonstrated valid delay across both provided profiles was 90 ms at
1.88x bandwidth overhead.