# Network Architecture

## Network Topology

Topology assumptions:

- single-hop Ethernet
- switched Gigabit LAN
- no routing
- no NAT
- no Wi-Fi bridges
- standard MTU 1500
- no jumbo frames


## IP Addressing

Static schema:

```text
10.0.0.XY
```

| Field | Meaning |
|---|---|
| X | Team ID (1–8) |
| Y | Node ID (1–3) |

Example:

```text
Team 4 Node 2 → 10.0.0.42
```


## Discovery Mechanism

Discovery SHALL use UDP multicast.

| Parameter | Value |
|---|---|
| Group | 239.192.88.100 |
| Port | 47200 |
| IP_MULTICAST_LOOP | 0 |

`IP_MULTICAST_LOOP = 0` SHALL disable sender-local multicast loopback only.


## Security Assumptions

The synchronization protocol assumes:

- trusted LAN operation
- fail-stop node behavior
- bounded congestion
- non-malicious traffic

The protocol provides NO:

- authentication
- encryption
- replay protection
- Byzantine fault tolerance
- confidentiality guarantees

Deployment outside trusted laboratory environments is NOT supported.


