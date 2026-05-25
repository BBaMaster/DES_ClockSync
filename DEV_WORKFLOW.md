# DRS Development Workflow

## Two-Loop Architecture

Development runs on Windows with WSL2 as the build environment and one Raspberry Pi 4B available over SSH.

### Inner Loop — WSL2, x86_64 (seconds per cycle)
**Edit → compile → unit test → iterate**

All platform-independent logic is unit-tested here without hardware. This is where the agentic loop spends most of its time.

### Outer Loop — RPi via SSH (minutes per cycle)
**Cross-compile → deploy → run → read telemetry → iterate**

Hardware-specific behavior (SO_TIMESTAMPING, SCHED_FIFO, GPIO) is validated on the actual target.

---

## Project Structure

```
DES_ClockSync/
├── src/
│   ├── main.c
│   ├── clock.h / clock.c         # Virtual clock: Q32.32 rate/offset, atomics
│   ├── protocol.h / protocol.c   # 64-byte packet: serialize, deserialize, CRC32
│   ├── sync.h / sync.c           # PI controller, min-delay filter, T1-T4 math
│   ├── election.h / election.c   # State machine, Bully algorithm
│   ├── gpio.h / gpio.c           # GPIO 18/23 (HAL — stubbed on host build)
│   ├── net.h / net.c             # UDP socket, SO_TIMESTAMPING, multicast
│   └── telemetry.h / telemetry.c # Lock-free ring buffer + UDP drain thread
├── tests/
│   ├── main_test.c               # Test runner (no external deps)
│   ├── test_protocol.c           # Serialize/deserialize, CRC32, endian
│   ├── test_clock.c              # Q32.32 math, virtual clock formula
│   ├── test_sync.c               # PI controller, min-delay filter, T1-T4 calc
│   └── test_election.c           # State machine transitions
├── scripts/
│   ├── deploy.sh                 # scp binary + systemd restart on RPi
│   └── telemetry_listen.py       # Windows-side UDP listener (prints CSV to stdout)
├── CMakeLists.txt
└── toolchain-aarch64.cmake       # Cross-compile: aarch64-linux-gnu-gcc
```

---

## HAL Stubs (Enable Host Testing)

Three functions are conditionally compiled via `-DPLATFORM=host`:

| Function | Host (WSL2) | RPi |
|---|---|---|
| `gpio_set(pin, val)` | no-op | sysfs write |
| `net_get_rx_timestamp(msg)` | `clock_gettime` software time | `SO_TIMESTAMPING` cmsg |
| `rt_thread_setup()` | no-op | `SCHED_FIFO` + `CPU_SET(3)` + `mlockall` |

Everything else — PI math, packet parsing, state machine, virtual clock — is identical on both platforms.

---

## What Unit Tests Cover (WSL2, no hardware needed)

| Test file | Covers |
|---|---|
| `test_protocol.c` | serialize↔deserialize round-trip, CRC32, endian byte order, magic/version, padding zeroed |
| `test_clock.c` | Q32.32 multiply, `read_global_time()` formula, monotonicity |
| `test_sync.c` | offset/delay from T1–T4 fixtures, min-delay filter accept/reject, PI step vs slew decision, integrator clamp |
| `test_election.c` | state transitions, holdover timeout, lower-ID demotion trigger |

---

## What RPi Tests Cover

- `SCHED_FIFO` priority 85 on Core 3: verified via `/proc/<pid>/stat`
- `SO_TIMESTAMPING` kernel timestamps present in cmsg
- GPIO 18 pulse toggling at ~1 Hz (self-test via GPIO input read loop)
- GPIO 23 transitions HIGH on startup → LOW after convergence (single-node: LEADER, offset ≈ 0)
- Telemetry UDP packets arriving on Windows listener
- Memory locked: `VmLck > 0` in `/proc/<pid>/status`

---

## Telemetry Design

**On RPi (non-RT drain thread):**
- RT sync thread writes fixed-size records to a lock-free ring buffer (power-of-2, atomic head/tail)
- Separate low-priority drain thread sends records via UDP to the Windows machine on port 4242
- Ring buffer write is a single compare-and-exchange — no blocking, no allocation

**On Windows (`scripts/telemetry_listen.py`):**
- `socket.recvfrom()` loop prints CSV: `timestamp_ns, state, offset_ns, rtt_ns, rate_q32`
- Run in WSL2 or native Python; output piped to a file for analysis

---

## Build Commands (run from Windows via `wsl ...`)

### Inner loop
```bash
wsl cmake -S . -B build-host -DPLATFORM=host
wsl cmake --build build-host
wsl ./build-host/tests/drs_tests
```

### Outer loop
```bash
# Cross-compile
wsl cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake
wsl cmake --build build-arm

# Deploy
wsl bash scripts/deploy.sh <rpi-ip>

# Capture telemetry for 30 s
wsl python3 scripts/telemetry_listen.py --duration 30
```

---

## Implementation Order & Commit Strategy

Each step below ends with a commit **before moving to the next step**. Commits are made as soon as the step's tests pass — not batched at the end. One logical unit per commit (one module, or one tightly related pair of files). Planning documents and workflow files never enter git history.

1. `CMakeLists.txt` + `toolchain-aarch64.cmake` + minimal `tests/` scaffold → confirm WSL2 builds → **commit**
2. `.github/workflows/ci.yml` — GitHub Actions: build host target, run tests, cross-compile ARM → CI green on GitHub → **commit**
3. `protocol.h/c` + `test_protocol.c` → all protocol tests pass → **commit**
4. `clock.h/c` + `test_clock.c` → all clock tests pass → **commit**
5. `sync.h/c` + `test_sync.c` → all sync tests pass → **commit**
6. `election.h/c` + `test_election.c` → all election tests pass → **commit**
7. `net.h/c` (with host stub) → compiles on host → **commit**
8. `gpio.h/c` (with host stub) → compiles on host → **commit**
9. `telemetry.h/c` → compiles on host → **commit**
10. `main.c` → full binary links and runs on host → **commit**
11. `scripts/deploy.sh` + `scripts/telemetry_listen.py` → **commit**
12. RPi hardware validation (no new code commits unless fixes needed)

---

## Verification Checklist

- [ ] `drs_tests` reports 0 failures across all 4 suites
- [ ] `ssh pi "cat /proc/$(pgrep drs_sync)/status | grep VmLck"` → nonzero
- [ ] `ssh pi "cat /proc/$(pgrep drs_sync)/task/*/stat"` → CPU column shows 3
- [ ] Telemetry stream shows `state=LEADER`, `offset_ns` near 0
- [ ] GPIO 23 transitions LOW within 10 s of startup
