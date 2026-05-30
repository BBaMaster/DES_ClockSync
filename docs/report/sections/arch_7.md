# Real-Time Scheduling

Synchronization thread SHALL:

- use SCHED_FIFO
- use priority 85
- remain pinned to Core 3

The synchronization loop MUST periodically yield using:

```text
clock_nanosleep()
```

to prevent:

- softirq starvation
- scheduler deadlock
- SSH lockout


