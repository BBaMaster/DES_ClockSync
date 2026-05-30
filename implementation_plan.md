# Code Review & Fix Implementation Plan

I have reviewed the codebase in the `src/` directory and cross-referenced it with `drs_architecture_reviewed_fixed_v_21.md`. While the code is well-structured, there are **four grave architectural violations** that fundamentally break the synchronization math, hardware pulse precision, and protocol compliance. 

I propose creating a new git branch to fix these issues. Please review this plan.

## Proposed Changes

### 1. Clock Domain Mixing in Timers (The `clock_nanosleep` requirement)
**The Mistake**: `main.c` relies on `timerfd` with `CLOCK_MONOTONIC` to schedule the GPIO 18 pulses and internal ticks, but it feeds these timers absolute timestamps calculated from `CLOCK_MONOTONIC_RAW`. Because `CLOCK_MONOTONIC` is actively slewed by NTP/systemd-timesyncd while `RAW` is not, the timer fires at the wrong physical time, causing severe drift and jitter on the pulse edge. Additionally, `epoll_wait`'s millisecond granularity destroys the <20 µs pulse edge requirement.
**The Fix**: 
- Remove `timerfd` and `epoll_wait` from the synchronization hot-path.
- Implement a high-precision spin-sleep loop using `clock_nanosleep(CLOCK_MONOTONIC_RAW, ...)` to sleep until just before the next event (pulse edge, heartbeat, sync tick), fulfilling the explicit yielding requirement from Section 7 of the spec.
- Handle socket reads via `recv(..., MSG_DONTWAIT)` directly within this loop.

### 2. Wire Protocol Timestamp Domain
**The Mistake**: `main.c` translates `T1`, `T2`, and `T3` into the local *virtual clock* domain before sending them on the wire (lines 189, 215). The architecture strongly explicitly forbids this (Section 4.6: *"All timestamps SHALL use: CLOCK_MONOTONIC_RAW nanoseconds"*). Sending pre-translated virtual timestamps breaks interoperability with nodes following the spec.
**The Fix**:
- Send raw `mono_raw_ns()` timestamps on the wire for T1, T2, and T3.
- Move the virtual clock translation strictly to the receiving logic where `theta` is calculated.

### 3. CRC32 Validation Coverage
**The Mistake**: `protocol.c` computes the CRC32 only over the first 50 bytes of the packet (`crc32_ieee(buf, 50)`). This leaves the 12 reserved padding bytes completely unprotected, violating Section 4.8 ("*Reserved padding bytes SHALL... be included in CRC validation*"). There is also a size contradiction in the spec (the table sizes add up to 66 bytes, but 64 bytes is strictly mandated).
**The Fix**: 
- Standardize the padding size to 10 bytes to strictly satisfy the 64-byte total size constraint.
- Move the CRC32 placement to the very end of the 64-byte packet (bytes 60-63), allowing it to validate the entire preceding 60 bytes including padding, standardizing the ethernet frame approach.

### 4. Implementation of `SO_TIMESTAMPING`
**The Mistake**: `net.c` completely ignores the mandatory `SO_TIMESTAMPING` API, using user-space `mono_raw_ns()` after `recv` and before `sendto` (which introduces severe scheduler latency jitter). 

## User Review Required

> [!WARNING]
> **Contradiction in Spec vs Linux Kernel regarding SO_TIMESTAMPING**
> The architecture dictates that `SO_TIMESTAMPING` is mandatory, and that *all* timestamps must be `CLOCK_MONOTONIC_RAW`. However, as the original developer correctly noted in a comment, Linux's fallback kernel software timestamping uses `CLOCK_REALTIME`. 
> 
> **Question**: Should I implement `SO_TIMESTAMPING` strictly (which will introduce `CLOCK_REALTIME` NTP steps into the math, breaking the PI controller), OR should I keep the current user-space `mono_raw_ns()` approach but mitigate scheduler latency by polling non-blocking sockets in the new `clock_nanosleep` tight loop? 

## Open Questions

1. Do you want me to proceed with implementing `SO_TIMESTAMPING` despite the `CLOCK_REALTIME` kernel limitation, or keep the `mono_raw_ns()` user-space timestamping strategy?
2. Does the CRC32 relocation (to the end of the packet) sound acceptable to resolve the padding validation requirement?

## Verification Plan
1. Compile the code using CMake (both host and arm if applicable).
2. Ensure the test suites still pass.
3. Review the diffs to guarantee `timerfd` is completely removed and the packet wire format outputs raw nanoseconds.
