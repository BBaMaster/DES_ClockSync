# Hardware & Operating Environment

## Supported Hardware

Target platform:

- Raspberry Pi 4B
- Quad-core Cortex-A72
- Gigabit Ethernet
- PREEMPT_RT Linux kernel

Recommended:

- passive cooling
- fixed thermal conditions
- wired Ethernet only


## GPIO Hardware Interface

### GPIO 18 — Synchronization Pulse

GPIO 18 SHALL generate:

- a 10 ms HIGH pulse
- once per global second

The rising edge SHALL define the global second boundary.

The pulse SHALL remain active in all operational states except:

```text
GROUND
```

### Timing Constraints

| Property | Requirement |
|---|---|
| Max pulse edge jitter | &lt;20 µs |
| Pulse source | Virtual global clock only |
| Pulse generation thread | RT synchronization thread |


### GPIO 23 — Synchronization Health Indicator

GPIO 23 SHALL indicate synchronization health.

| State | Meaning |
|---|---|
| LOW | Stable synchronized operation |
| HIGH | Startup, degraded sync, recovery, calibration, or fault |

GPIO 23 SHALL transition LOW only after:

```text
|Offset| &lt; 100 µs
```

for a continuous:

```text
10-second interval
```


## External Verification

Recommended instrumentation:

- multi-channel digital oscilloscope
- logic analyzer

Minimum sample rate:

```text
≥ 1 MHz
```

Measurement criteria:

- physical delta between GPIO 18 rising edges


## Kernel & OS Hardening

### Required Kernel Parameters

```text
isolcpus=3
nohz_full=3
rcu_nocbs=3
```

### Scheduling Requirements

Synchronization thread SHALL:

- execute exclusively on Core 3
- use `SCHED_FIFO`
- use priority 85

### CPU Frequency Requirements

CPU governor SHALL be:

```text
performance
```

Turbo and dynamic voltage scaling SHOULD be disabled.

### Memory Requirements

All synchronization process memory SHALL be locked using:

```text
mlockall(MCL_CURRENT | MCL_FUTURE)
```

No page faults SHALL occur in the synchronization hot path.

### Time Service Isolation

The following services SHALL be disabled:

- systemd-timesyncd
- chronyd
- ntpd
- ptp4l
- phc2sys


