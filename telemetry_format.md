# DRS Telemetry Packet Format

## Transport

- **Protocol:** UDP unicast
- **Port:** 4242
- **Direction:** each Pi → visualizer machine IP (passed as second CLI arg to `drs_sync`)
- **Rate:** one packet per sync exchange (follower: ~10 Hz, leader: ~20 Hz at 50 ms tick)

## Packet Layout

Fixed 40-byte binary, **little-endian** (native RPi byte order, no byte-swap).

| Offset | Size | Type    | Field          | Description |
|--------|------|---------|----------------|-------------|
| 0      | 8    | int64   | `timestamp_ns` | `CLOCK_MONOTONIC_RAW` on the sending node (nanoseconds) |
| 8      | 4    | int32   | `state`        | Node state enum (see below) |
| 12     | 8    | int64   | `offset_ns`    | Measured clock offset from leader (nanoseconds). 0 for leader packets. |
| 20     | 8    | int64   | `rtt_ns`       | Round-trip time of the sync exchange (nanoseconds). 0 for leader packets. |
| 28     | 8    | int64   | `rate_q32`     | Virtual clock rate as Q32.32 fixed-point (see below) |
| 36     | 4    | uint32  | `node_id`      | Sender's node ID (matches last octet of its IP: `10.0.0.<node_id>`) |

## State Values

| Value | Name          | Meaning |
|-------|---------------|---------|
| 0     | `GROUND`      | Startup hold-off (waiting ~1 s before participating) |
| 1     | `CALIBRATION` | Measuring self-latency (one-shot at startup) |
| 2     | `LISTEN`      | Waiting to hear a leader before running for election |
| 3     | `CANDIDATE`   | Running for election (transitions to LEADER immediately if no lower-ID node seen) |
| 4     | `FOLLOWER`    | Synchronized to leader — offset_ns and rtt_ns are valid |
| 5     | `LEADER`      | This node is the time reference — offset_ns and rtt_ns are always 0 |
| 6     | `HOLDOVER`    | Leader lost; free-running on last known rate for up to 10 s |

## Rate Field (Q32.32 Fixed-Point)

`rate_q32` is a signed 64-bit integer. The integer value `2^32 = 4294967296` represents a rate of exactly 1.0 (nominal, no adjustment).

To convert to a human-readable PPM deviation:

```python
RATE_ONE = 1 << 32  # 4294967296
ppm = (rate_q32 - RATE_ONE) / RATE_ONE * 1_000_000
```

Normal range: **±1000 ppm** (clamped by the PI controller).

## Python Unpack Snippet

```python
import struct

RECORD_SIZE = 40

STATE_NAMES = {
    0: "GROUND", 1: "CALIBRATION", 2: "LISTEN",
    3: "CANDIDATE", 4: "FOLLOWER", 5: "LEADER", 6: "HOLDOVER"
}

def unpack(data: bytes) -> dict:
    ts, state, offset, rtt, rate, node_id = struct.unpack_from("<qiqqqI", data)
    return {
        "node_id":      node_id,
        "timestamp_ns": ts,
        "state":        STATE_NAMES.get(state, state),
        "offset_ns":    offset,
        "offset_us":    offset / 1000,
        "rtt_ns":       rtt,
        "rtt_us":       rtt / 1000,
        "rate_ppm":     (rate - (1 << 32)) / (1 << 32) * 1e6,
    }
```

> **Note:** `timestamp_ns` is `CLOCK_MONOTONIC_RAW` from each node's local clock — it is **not** comparable between nodes. Use `offset_ns` to understand inter-node alignment.
