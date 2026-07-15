# Real-Time Media Transport

A loss-tolerant UDP transport using overlapping pair and triple XOR FEC for recovery under packet loss, delay, reordering, and duplication.

## Deliverables

- `sender.c` — Sender and FEC generation
- `receiver.c` — Receiver and iterative loss recovery
- `Makefile` — Builds both executables
- `RUNLOG.md` — Experimental progression and benchmark results
- `NOTES.md` — Final design, grading delay, and limitations
- `SUMMARY.html` — Detailed system architecture and design explanation

## Build

```bash
make
```

## Test for the benchmark @91ms :
```bash
make clean & make
python3 run.py --profile profiles/A.json --delay_ms 91
python3 run.py --profile profiles/B.json --delay_ms 91
```