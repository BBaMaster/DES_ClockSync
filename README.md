[![Unit Tests](https://github.com/BBaMaster/DES_ClockSync/actions/workflows/ci.yml/badge.svg)](https://github.com/BBaMaster/DES_ClockSync/actions/workflows/ci.yml)
[![Build Report PDF](https://github.com/BBaMaster/DES_ClockSync/actions/workflows/build-report.yml/badge.svg)](https://github.com/BBaMaster/DES_ClockSync/actions/workflows/build-report.yml)

# DRS — Distributed Clock Synchronization

High-precision distributed clock synchronization for a cluster of Raspberry Pi 4B nodes. All nodes maintain a shared virtual clock aligned within **< 100 µs**, verified via GPIO pulses on a logic analyzer.

Pure user-space C on PREEMPT_RT Linux. No NTP, no GPS, no kernel modules.

## How it works

One node is elected leader (lowest node ID wins). The leader broadcasts heartbeats; followers send timestamp exchange requests every 50 ms. Using four timestamps (T1–T4), each follower computes its clock offset and round-trip delay relative to the leader.

A PI controller adjusts a **virtual clock** — a software layer on top of `CLOCK_MONOTONIC_RAW` — by tuning a Q32.32 fixed-point rate multiplier and a nanosecond offset. The host OS clock is never touched. A min-delay filter (window of 10 samples) rejects scheduler spikes before feeding the controller. If the offset exceeds 1 ms for three consecutive samples a hard step is applied; otherwise the clock is slewed at up to 1000 ppm.

The sync thread runs `SCHED_FIFO` priority 85 on isolated CPU core 3. GPIO 18 pulses once per second so nodes can be compared on a logic analyzer. GPIO 23 goes LOW once the offset has stayed below 100 µs for 10 continuous seconds.

---

## Requirements

### Raspberry Pi (each node)
- Raspberry Pi 4B with PREEMPT_RT kernel
- Static IP: `10.0.0.XY` (X = team ID, Y = node ID)
- Gigabit Ethernet, wired only
- `aarch64-linux-gnu-gcc` cross-compiler on your build machine

### Build machine (Windows + WSL2)
- WSL2 with `gcc`, `cmake`, `aarch64-linux-gnu-gcc`
- Python 3 (for telemetry listener)

---

## One-time RPi setup

### Static IP

Switch to `systemd-networkd` and create a network config file:

```bash
sudo systemctl disable --now NetworkManager
sudo systemctl enable --now systemd-networkd
```

Check the interface name first (`eth0` or `end0`):

```bash
ip link show
```

Create `/etc/systemd/network/eth0.network` (adjust `Name=` and `Address=` to match your node):

```ini
[Match]
Name=eth0

[Network]
Address=10.0.0.11/24
```

```bash
sudo systemctl restart systemd-networkd
ip addr show eth0   # verify
```

---

On each Pi, disable competing time services and isolate CPU core 3:

```bash
sudo systemctl disable --now systemd-timesyncd chronyd ntpd ptp4l phc2sys
```

Add to `/boot/firmware/cmdline.txt` (append to the existing single line — no newline):

```
isolcpus=3 nohz_full=3 rcu_nocbs=3
```

Set CPU governor to performance persistently via a systemd one-shot service:

```bash
sudo nano /etc/systemd/system/cpu-performance.service
```

```ini
[Unit]
Description=Set CPU governor to performance

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now cpu-performance
```

Reboot and verify:

```bash
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor  # should print: performance
cat /sys/devices/system/cpu/isolated                        # should print: 3
```

Install the systemd service unit:

```ini
# /etc/systemd/system/drs_sync.service
[Unit]
Description=DRS Synchronization Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStartPre=-/sbin/ip route add 224.0.0.0/4 dev eth0
ExecStart=/usr/local/bin/drs_sync <node_id>
Restart=on-failure
RestartSec=5s
CPUAffinity=3
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=85
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
```

Replace `<node_id>` with the node's numeric ID (e.g. `2` for `10.0.0.X2`).

```bash
sudo systemctl daemon-reload
sudo systemctl enable drs_sync
```

---

## Build

### Host build + tests (WSL2, no hardware needed)

```bash
wsl cmake -S . -B build-host -DPLATFORM=host
wsl cmake --build build-host
wsl ./build-host/drs_tests
```

All 4 test suites must report 0 failures before deploying.

### Cross-compile for RPi

```bash
wsl cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake
wsl cmake --build build-arm
```

---

## Deploy

```bash
wsl bash scripts/deploy.sh <rpi-ip>
```

This copies `build-arm/drs_sync` to `/usr/local/bin/drs_sync` on the Pi and restarts the service.

Example — deploy to node at `10.0.0.12`:

```bash
wsl bash scripts/deploy.sh 10.0.0.12
```

---

## Usage

The binary takes a node ID and an optional telemetry destination IP:

```bash
drs_sync <node_id> [telem_dest_ip]
```

When running as a service the systemd unit handles this. To run manually on the Pi for testing:

```bash
sudo /usr/local/bin/drs_sync 2 10.0.0.1
```

The lowest node ID in the cluster automatically becomes the leader. All other nodes follow it.

---

## Telemetry

Each node sends fixed 40-byte UDP packets (little-endian) to port 4242. Full packet layout, state enum values, Q32.32 rate field, and a Python unpacker are documented in [telemetry_format.md](telemetry_format.md).

On your Windows machine, listen for telemetry from any Pi:

```bash
python scripts/telemetry_listen.py
```

Output is CSV on stdout:

```
timestamp_ns,state,offset_ns,rtt_ns,rate_q32
1234567890000,FOLLOWER,-342,18500,4294967296
```

Capture 60 seconds to a file:

```bash
python scripts/telemetry_listen.py --duration 60 > sync_log.csv
```

---

## Verify a running node

```bash
RPI=10.0.0.12
PID=$(ssh pi@$RPI pgrep drs_sync)

# Memory locked
ssh pi@$RPI "grep VmLck /proc/$PID/status"

# Running on core 3
ssh pi@$RPI "grep ^processor /proc/$PID/task/*/stat"

# Telemetry flowing (run on Windows)
wsl python3 scripts/telemetry_listen.py --duration 5
```

---

## GPIO

| Pin | Signal |
|-----|--------|
| GPIO 18 | 10 ms HIGH pulse once per second (rising edge = second boundary) |
| GPIO 23 | LOW = synced (offset < 100 µs for 10 s), HIGH = not synced |

Connect GPIO 18 across all nodes to a logic analyzer to measure physical alignment.
