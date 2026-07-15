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