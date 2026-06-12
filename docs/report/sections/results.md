# Experimental Results & Verification

All tests were executed on Raspberry Pi 4B nodes running a `PREEMPT_RT` Linux kernel, connected via wired Gigabit Ethernet.

---

## Two-Node Baseline (Own Implementation)

Two nodes running the DRS implementation were connected directly via Ethernet. Node 1 (lowest ID) was elected leader; Node 2 synchronized as a follower.

**GPIO 18** from both nodes was connected to a logic analyzer. The rising-edge offset between the two 1 Hz pulses was measured as the ground truth for physical synchronization accuracy.

- Physical clock offset between the two nodes: **< 3 µs**
- GPIO 23 on Node 2 transitioned LOW (stable sync confirmed) within seconds of startup
- The PI controller settled quickly, with the Q32.32 rate multiplier stabilizing at a small correction factor corresponding to the crystal frequency drift between the two boards

The **< 100 µs** project requirement was comfortably met under wired conditions.

---

## Six-Node Cross-Team Cluster

The node was integrated into a six-node cluster running implementations from different teams. All nodes communicated over the same wired Ethernet segment using the shared multicast group and wire protocol.

- Maximum observed physical clock offset across all six nodes: **< 40 µs**
- The DRS node operated correctly as both leader and follower in the mixed cluster
- No protocol incompatibilities were observed; the fixed 64-byte packet format and CRC32 validation ensured interoperability

The result demonstrates that the synchronization goal of **< 100 µs** holds across a heterogeneous multi-team cluster.

---

## Summary

| Test | Setup | Measured Offset | Result |
|------|-------|----------------|--------|
| Baseline | 2 nodes, own implementation, wired | **< 3 µs** | PASSED |
| Cross-team cluster | 6 nodes, mixed implementations, wired | **< 40 µs** | PASSED |
