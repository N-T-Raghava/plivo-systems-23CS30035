# Notes

* The final design uses proactive forward error correction, sending every source frame once together with one XOR parity packet for every pair of frames and one XOR parity packet for every group of three frames.
* Each packet carries a 4-byte network-order header identifying its type and sequence or parity-group base, followed by the 160-byte payload.
* The receiver uses these sequence numbers to tolerate reordering and duplication while storing original frames, pair parity, and triple parity independently.
* When exactly one frame is missing from a pair or triple, the receiver reconstructs it with XOR and immediately reuses the recovered frame to unlock further recovery opportunities.
* Frames are sent to the player as soon as they are received or reconstructed, so `DELAY_MS` acts as the available network and recovery budget rather than an additional receiver-side holding period.
* The final design uses no feedback or retransmission because a resend would require a request and reply to cross the hostile network before the frame deadline.
* The measured bandwidth overhead is 1.88x, below the 2.0x limit.
* The lowest demonstrated valid delay across both provided profiles is 90 ms, **but it places Profile B exactly at the 1.00% miss-rate limit**.
* **I recommend grading at `delay_ms = 91`, where Profile A had 0.00% misses and Profile B had 0.87% misses while preserving a lower delay than the 100 ms test point.** (Logs are in the RUNLOG.md)
* The design can break under sufficiently large burst losses, correlated loss of source and parity packets, or delay spikes that push required data beyond the available playout deadline.