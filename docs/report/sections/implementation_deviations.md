# Implementation Deviations

This chapter documents the technical and architectural deviations of the actual C implementation from the theoretical specification defined in the original project Architecture Anchor. These changes were necessary to ensure system stability, loop damping, mathematical continuity, and real-time safety under `PREEMPT_RT` Linux.

---

## 1. Virtual Clock Continuity (Piecewise Linear Base-Rebase)

### Theoretical Spec (§1.5)
The architecture specification describes the virtual clock as a single, absolute linear mapping:
$$T_{\text{global}} = (T_{\text{local}} \times \text{Rate}) + \text{Offset} - \text{Latency}$$
Where $T_{\text{local}}$ represents the raw `CLOCK_MONOTONIC_RAW` nanoseconds since kernel boot.

### The Implementation Deviation
If the system clock multiplier (`Rate`) is adjusted directly using this absolute formula, it scales the entire time accumulator since boot ($T_{\text{local}}$). After a node has been booted for just one hour ($3.6 \times 10^{12}$ ns), a microscopic adjustment in `Rate` of $1\ \text{ppm}$ ($1 \times 10^{-6}$) will trigger a sudden, discontinuous time jump of:
$$\Delta T = 3.6 \times 10^{12}\ \text{ns} \times 10^{-6} = 3.6\ \text{ms}$$
A time jump of $3.6\ \text{ms}$ instantly breaks the $< 100\ \mu\text{s}$ synchronization constraint and violates time monotonicity.

To prevent these discontinuous phase jumps, the codebase (in `src/clock.h` and `src/clock.c`) implements a **piecewise linear base-rebase model**:
$$T_{\text{global}} = T_{\text{base\_global}} + \frac{(T_{\text{local}} - T_{\text{base\_local}}) \times \text{rate\_q32}}{2^{32}} - \text{latency}$$
Whenever the PI controller adjusts the frequency rate multiplier or applies a hard phase step, the clock is *rebased*:
1.  The current global time is computed.
2.  $T_{\text{base\_local}}$ is set to the current local raw time.
3.  $T_{\text{base\_global}}$ is set to the computed current global time.
4.  The new `rate_q32` or offset is applied.

This ensures that the virtual clock is mathematically continuous at the boundary of rate modifications, keeping phase transitions smooth.

---

## 2. Multi-Variable Coherency (Sequential Lock vs. Raw Atomics)

### Theoretical Spec (§9)
The specification asserts: *"Offset and Rate updates SHALL use atomic 64-bit operations... Reader access SHALL avoid mutex blocking."*

### The Implementation Deviation
While simple atomic 64-bit loads and stores prevent raw word corruption, they do not guarantee coherency across multiple distinct variables. Because `rate_q32`, $T_{\text{base\_local}}$, and $T_{\text{base\_global}}$ are mathematically coupled, concurrent reader threads (such as the telemetry logger or the GPIO 18 pulse thread) could perform separate atomic reads in the middle of a writer rebase. This results in a **torn read**—for example, reading a newly updated `rate_q32` combined with an old $T_{\text{base\_local}}$—which leads to massive timing errors in the calculated global time.

To solve this, the codebase implements a lock-free **Sequential Lock (SeqLock)** using an atomic sequence counter:
```c
typedef struct {
    uint64_t base_local;
    uint64_t base_global;
    int64_t  rate_q32;
    int64_t  latency;
    _Atomic unsigned seq; // Sequence counter
} VirtualClock;
```

*   **The Writer (Sync Loop):** Increments the sequence counter `seq` (making it odd) before modifying the clock parameters, performs the rebase, and increments `seq` again (making it even) after completion.
*   **The Reader (Telemetry/Pulse):** Reads `seq` before copying the parameters, performs the copy using standard atomic instructions, reads `seq` again, and retries if the sequence number was odd or if it changed during the read.

This lock-free SeqLock pattern guarantees that readers always obtain a coherent snapshot of the coupled variables, ensuring writer precedence so the real-time thread is never blocked by lower-priority reader threads.

---

## 3. PI Control Loop Stability (Positional vs. Incremental)

### Theoretical Spec (§5.6)
The specification defines an incremental frequency controller:
$$\text{Rate}_{\text{new}} = \text{Rate}_{\text{old}} + K_p \cdot \theta + K_i \cdot \int \theta dt$$

### The Implementation Deviation
In a clock synchronization loop, frequency adjustments are integrated over time by the clock accumulator to produce the phase (time offset). If an incremental frequency controller is used, the error $\theta$ is integrated twice (once to calculate the frequency correction, and a second time by the clock accumulator to calculate time). This creates a **double-integrator system**, which is inherently unstable and prone to phase oscillations and overshoots.

To guarantee loop damping and stability, the codebase (in `src/sync.c`) implements a **positional PI controller**:
$$\text{Rate}_{\text{new}} = \text{Nominal\_Rate} + \text{kp\_ppm} + \text{integrator\_ppm}$$
Where the proportional correction (`kp_ppm`) is calculated directly from the current phase error, and only the integral term accumulates historical errors over time.

Additionally, to relate gain constants to physical timing, the controller converts the raw phase offset $\theta$ (in nanoseconds) into a dimensionless frequency correction in parts-per-million (PPM) using the update interval ($\Delta t = 50\ \text{ms}$):
$$\text{kp\_ppm} = \frac{K_p \cdot \theta_{\text{ns}} \cdot 10^6}{\text{SYNC\_PERIOD\_NS}}$$
This ensures the loop gains are properly normalized to the physical timing of the network ticks.

---

## 4. Real-Time Safety (Startup vs. Transition Calibration)

### Theoretical Spec (§5.4)
The specification requires loopback latency calibration to re-run during startup, after leader election, after holdover expiration, and after synchronization fault recovery.

### The Implementation Deviation
The loopback calibration routine (`calibrate_loopback`) transmits and receives 50 network probe packets, waiting on socket `select()` timeouts to filter network noise. This process takes several milliseconds to complete. 

If this blocking calibration routine were executed during active runtime state transitions (such as recovering from a leader failover or holdover), it would block the main real-time synchronization loop (`sync_thread`) on core 3. This would violate the `SCHED_FIFO` scheduler timing constraints, cause missed Ethernet packet deadlines, and trigger kernel watchdog warnings.

To maintain real-time safety:
*   The calibration is executed **once during startup** in `main.c` on the main non-RT thread.
*   The calibration parameters are saved, and the real-time `sync_thread` is spawned afterwards.
*   Active state transitions inside the real-time loop are designed to be completely non-blocking, ensuring core 3 is never stalled on I/O.
