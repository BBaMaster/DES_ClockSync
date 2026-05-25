# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

**DRS (Distributed Real-time Synchronization)** is a high-precision distributed clock synchronization system for a cluster of Raspberry Pi 4B nodes. The goal is to keep all nodes' virtual clocks aligned within **< 100 µs**, measured physically via GPIO pulses observed on a logic analyzer.

This is a **pure user-space C/C++ implementation** targeting **PREEMPT_RT Linux** on Raspberry Pi 4B (Cortex-A72). No kernel modules. No NTP/PTP/GPS. No external time authority. The cluster is self-synchronizing and internal-only.

The full architecture specification is in [drs_architecture_reviewed_fixed_v_21.md](drs_architecture_reviewed_fixed_v_21.md).

---

## Architecture

### Core Concept

Each node runs the same binary. One node is elected **leader** (lowest NodeID wins via modified Bully algorithm). Followers exchange timestamps with the leader using a PTP-style 4-timestamp exchange (T1–T4) over UDP multicast on a single-hop Gigabit Ethernet LAN.

Synchronization adjusts a **virtual clock layer only** — the host OS clock (`CLOCK_REALTIME`) is never touched. All timestamps use `CLOCK_MONOTONIC_RAW`.

### Virtual Clock

```
Tglobal = (Tlocal_raw × Rate) + Offset − LatencyCorrection_ns
```

- `Rate` is Q32.32 fixed-point (no floating-point in hot path)
- `Offset` is phase correction in nanoseconds
- `LatencyCorrection_ns` is measured once per calibration phase

### Node State Machine

`GROUND → CALIBRATION → LISTEN → CANDIDATE → LEADER or FOLLOWER`

Fallback: `HOLDOVER` (freerun for up to 10 s when leader is lost), then re-election.

### Synchronization Loop

- Runs exclusively on **CPU Core 3** (isolated via `isolcpus=3 nohz_full=3 rcu_nocbs=3`)
- `SCHED_FIFO` priority 85
- 50 ms exchange period, 100 ms leader heartbeat, 3-heartbeat timeout
- Uses `clock_nanosleep()` for yielding (prevents softirq starvation/SSH lockout)
- `mlockall(MCL_CURRENT | MCL_FUTURE)` — no page faults in hot path

### Control Algorithm

Dual-loop PI controller:
```
Rate_new = Rate_old + Kp·θ + Ki·∫θdt
```
Defaults: Kp=0.05, Ki=0.005. Max slew ±1000 ppm. Integrator clamped to ±1000 ppm.

Step mode (hard phase correction) requires 3 consecutive samples confirming |θ| > 1 ms.

### Min-Delay Filter

RTT window size = 10. Only samples within `min_RTT + 10 µs` are accepted to reject scheduler spikes and interrupt bursts.

---

## Wire Protocol

- Fixed 64-byte UDP packets, Big Endian
- Magic: `0x44525354` ("DRST"), Version 1
- Message types: `ANNOUNCE (0x01)`, `SYNC_REQ (0x02)`, `SYNC_RESP (0x03)`
- CRC32 with IEEE 802.3 polynomial, covers full 64 bytes including zero-padded reserved fields
- All fields serialized explicitly with `htons()`/`htonl()`/explicit 64-bit conversion — **no raw struct casting**
- Timestamps captured via `SO_TIMESTAMPING` (hardware NIC preferred, kernel software fallback)

### Network

- Multicast group: `239.192.88.100`, port `47200`
- `IP_MULTICAST_LOOP = 0` (suppress sender-local loopback only)
- Node IPs: `10.0.0.XY` where X=team ID, Y=node ID

---

## GPIO

- **GPIO 18**: 10 ms HIGH pulse once per global second (rising edge = second boundary). Jitter < 20 µs.
- **GPIO 23**: Sync health indicator. LOW = stable (|offset| < 100 µs for 10 continuous seconds). HIGH = any other state.

---

## Hard Constraints (Never Violate)

- **Never** call `clock_settime()`, `adjtimex()`, or step `CLOCK_REALTIME`
- **Never** use floating-point in the synchronization hot path
- **Never** allocate heap memory in the hot path
- **Never** do filesystem/console I/O in the sync loop
- **Never** use TCP for synchronization transport
- **Never** allow WLAN interrupts or Ethernet IRQs to execute on Core 3
- **Never** use mutex blocking in the sync loop — use atomic acquire/release for Rate/Offset updates

---

## Deployment

The binary deploys as a systemd service (`/usr/local/bin/drs_sync`) with `CPUAffinity=3`, `CPUSchedulingPolicy=fifo`, `CPUSchedulingPriority=85`, `LimitMEMLOCK=infinity`.

Disable before running: `systemd-timesyncd`, `chronyd`, `ntpd`, `ptp4l`, `phc2sys`.

CPU governor must be set to `performance`.
