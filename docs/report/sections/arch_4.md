# DRS Wire Protocol

## Packet Format

All packets SHALL:

- use fixed-size binary encoding
- use network byte order (Big Endian)
- avoid dynamic allocation in the hot path
- avoid text serialization

Raw struct casting is prohibited.

All fields SHALL be serialized explicitly using:

- `htons()`
- `htonl()`
- explicit 64-bit endian conversion


## Packet Size

All packets SHALL be exactly:

```text
64 bytes
```


## Packet Layout

| Field | Type | Size | Description |
|---|---|---|---|
| Magic | uint32 | 4 | Constant: 0x44525354 (“DRST”) |
| Version | uint8 | 1 | Protocol version |
| MsgType | uint8 | 1 | Message type |
| Flags | uint8 | 1 | Synchronization state flags |
| Reserved | uint8 | 1 | Reserved |
| Seq | uint16 | 2 | Rolling sequence number |
| NodeID | uint32 | 4 | Unique node ID |
| ElectionTerm | uint32 | 4 | Leader election epoch |
| T1 | uint64 | 8 | Follower transmit timestamp |
| T2 | uint64 | 8 | Leader receive timestamp |
| T3 | uint64 | 8 | Leader transmit timestamp |
| T4 | uint64 | 8 | Follower receive timestamp |
| CRC32 | uint32 | 4 | Packet integrity |
| Padding | uint8[12] | 12 | Reserved |
| Total | — | 64 | Total packet size |


## Message Types

| Value | Meaning |
|---|---|
| 0x01 | ANNOUNCE |
| 0x02 | SYNC_REQ |
| 0x03 | SYNC_RESP |


## Flags Field

| Bit | Meaning |
|---|---|
| 0 | LEADER |
| 1 | HOLDOVER |
| 2 | CALIBRATED |
| 3 | FAULT |
| 4-7 | Reserved |

Reserved bits SHALL be zero.


## Timestamp Semantics

| Timestamp | Meaning |
|---|---|
| T1 | Sync request send time |
| T2 | Kernel-captured receive timestamp |
| T3 | Kernel-captured transmit timestamp |
| T4 | Sync response receive timestamp |

All timestamps SHALL use:

```text
CLOCK_MONOTONIC_RAW nanoseconds
```


## Timestamping Mechanism

Mandatory Linux API:

```text
SO_TIMESTAMPING
```

Accepted modes:

1. hardware NIC timestamping
2. kernel software timestamping

The highest precision mode supported by the platform SHALL be selected automatically.


## CRC32

CRC32 SHALL use:

- IEEE 802.3 polynomial `0x04C11DB7`
- initial value `0xFFFFFFFF`
- reflected input and output

Reserved padding bytes SHALL:

- be zero-filled
- be included in CRC validation


