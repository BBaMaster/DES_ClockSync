# Project Definition

## Mission Statement

The DRS project establishes a deterministic, high-precision distributed global time base across a dynamic cluster of Raspberry Pi nodes operating exclusively in Linux user space.

The system shall maintain a physical synchronization delta of:

```text
Δtphysical &lt; 100 µs
```

as measured externally using GPIO-generated synchronization pulses observed on a logic analyzer or oscilloscope.

The architecture prioritizes:

- deterministic behavior
- operational simplicity
- fail-safe recovery
- real-time scheduling integrity
- minimal protocol complexity
- bounded timing behavior


## Core Design Philosophy

The synchronization subsystem SHALL follow the KISS principle:

```text
Keep It Simple, Stupid (KISS)
```

The protocol intentionally avoids:

- distributed consensus frameworks
- dynamic spanning trees
- kernel modifications
- external time authorities
- heavyweight serialization
- multi-hop routing
- dynamic routing protocols
- cryptographic processing in the synchronization hot path

The synchronization cluster is designed exclusively for:

- single-hop Layer-2 Gigabit Ethernet
- trusted laboratory environments
- fail-stop node behavior
- deterministic LAN conditions


## Revised Core Constraints

- Wired Gigabit Ethernet SHALL be the exclusive synchronization transport.
- WLAN MAY be used exclusively for out-of-band management and provisioning.
- CPU Core 3 SHALL be reserved exclusively for synchronization operations.
- WLAN interrupts SHALL NEVER execute on Core 3.
- Ethernet IRQs SHALL NEVER execute on Core 3.
- The Linux host system clock SHALL NEVER be modified.
- All synchronization adjustments SHALL occur exclusively inside the virtual clock layer.

Explicitly prohibited:

- `clock_settime()`
- `adjtimex()`
- `CLOCK_REALTIME` stepping
- kernel PLL interaction


## Synchronization Paradigm

The system implements:

```text
Internal Monotonic Synchronization
```

A single elected leader defines the cluster reference timeline.

Follower nodes estimate:

- phase offset
- frequency drift
- path delay

relative to the elected leader.

The synchronization domain is entirely internal.

The cluster SHALL NOT synchronize to:

- UTC
- NTP
- GPS
- PTP grandmasters
- wall-clock time


## Virtual Clock Model

The virtual clock mapping SHALL be defined as:

```text
Tglobal = (Tlocal_raw × Rate) + Offset − LatencyCorrection_ns
```

Where:

| Parameter | Description |
|---|---|
| Tlocal_raw | CLOCK_MONOTONIC_RAW timestamp |
| Rate | Fixed-point frequency scaling multiplier |
| Offset | Phase correction in nanoseconds |
| LatencyCorrection_ns | Static calibration compensation |

### Rate Representation

`Rate` SHALL be represented as signed 64-bit Q32.32 fixed-point.

Definitions:

| Value | Meaning |
|---|---|
| 0x00000001_00000000 | 1.0x nominal clock rate |
| +1000 ppm | Maximum positive slew |
| -1000 ppm | Maximum negative slew |

Floating-point arithmetic SHALL NOT be used inside the synchronization hot path.


## Time Base Definition

The synchronization engine SHALL operate exclusively on:

```text
CLOCK_MONOTONIC_RAW
```

All protocol timestamps SHALL represent:

```text
Absolute nanoseconds since local kernel boot.
```

The cluster timeline SHALL be defined relative to the elected leader's monotonic boot-time origin.

The system SHALL NOT use:

- Unix Epoch
- wall-clock time
- CLOCK_REALTIME

Optional wall-clock export layers MAY exist externally but remain outside synchronization scope.


