# Non-Functional Requirements

| ID | Requirement |
|---|---|
| NF-1 | Physical pulse delta &lt;100 µs |
| NF-2 | Pure user-space implementation |
| NF-3 | Jitter resilience under mixed traffic |
| NF-4 | Lock convergence within 10 s |
| NF-5 | 3-heartbeat hysteresis |
| NF-6 | Writer never blocked by readers |
| NF-7 | SCHED_FIFO priority 85 |
| NF-8 | mlockall memory locking |
| NF-9 | No hot-path disk or console I/O |
| NF-10 | Isolated Core 3 execution |
| NF-11 | Max slew = 1000 ppm |
| NF-12 | No dynamic heap allocation in hot path |
| NF-13 | No packet fragmentation |
| NF-14 | Deterministic packet parsing |
| NF-15 | No mutex contention in sync loop |
| NF-16 | Core 3 sanctity |


