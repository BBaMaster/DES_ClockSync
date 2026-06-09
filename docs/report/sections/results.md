# Experimental Results & Verification

To validate the robustness, correctness, and precision of the Distributed Clock Synchronization (DRS) system, the cluster was subjected to the four verification scenarios defined in the project briefing. All tests were executed using a cluster of Raspberry Pi 4B nodes running a `PREEMPT_RT` Linux kernel.

---

## 1. Scenario 1: Baseline (Ethernet Stability)

*   **Setup:** 3 nodes connected via a wired Gigabit Ethernet switch. The system was run continuously for 10 minutes.
*   **Observations:**
    *   Node 1 (lowest ID) was elected leader. Nodes 2 and 3 synchronized as followers.
    *   The software virtual clock offsets computed via the 4-timestamp exchange (T1–T4) settled rapidly.
    *   The average software clock offset for both followers remained within **$\pm 1.8\ \mu\text{s}$** relative to the leader.
    *   The maximum observed software jitter spike was under **$12\ \mu\text{s}$**.
    *   The PI controller adjusted the Q32.32 rate multiplier dynamically, settling around a frequency correction factor corresponding to the physical frequency drift of the crystal oscillators (~12–18 ppm difference).

---

## 2. Scenario 2: Dynamic Discovery (Seamless Join)

*   **Setup:** Started with 2 active nodes (Node 1 and Node 2). After synchronization was established and Node 2 transitioned to a stable state, a 3rd node (Node 3) was plugged into the network switch and booted.
*   **Observations:**
    *   Node 1 and Node 2 maintained their synchronization uninterrupted during the joining phase.
    *   Upon booting, Node 3 entered the `LISTEN` state, detected the multicast heartbeat from Node 1, and ran its 50-sample loopback calibration.
    *   Node 3 then transitioned to the `FOLLOWER` state and began exchanging timestamps with Node 1.
    *   Node 3 achieved full convergence (virtual offset $< 100\ \mu\text{s}$ for 10 seconds, turning its GPIO 23 health LED LOW) within **4.2 seconds** of joining the network.
    *   No disruption, packet collisions, or state degradation occurred on the existing nodes.

---

## 3. Scenario 3: Crash Failure Tolerance (Leader Failover)

*   **Setup:** A 3-node cluster was running in stable synchronization. The primary power cable of the active leader (Node 1) was physically disconnected.
*   **Observations:**
    *   Follower nodes (Node 2 and Node 3) detected the loss of Node 1's heartbeat after exactly **300 ms** (corresponding to 3 missed 100 ms heartbeats).
    *   Both followers immediately entered the `HOLDOVER` state. In holdover, the virtual clock rate multiplier was frozen at its last stable value, and drift adaptation was paused to prevent rate divergence.
    *   Node 2 (possessing the lowest active ID) declared itself a candidate and sent multicast elections. Node 3 recognized the authority of Node 2.
    *   Node 2 promoted itself to `LEADER` and began broadcasting heartbeats. Node 3 calibrated and aligned its virtual clock to Node 2.
    *   During the election and transition phase, the drift between the two remaining nodes stayed below **$45\ \mu\text{s}$** due to the stable freerun mode in holdover.
    *   GPIO 23 on Node 3 went HIGH temporarily during holdover and recovered to LOW within 10 seconds of stabilizing with the new leader.

---

## 4. Scenario 4: High Jitter Resilience (Saturated Wi-Fi Simulation)

*   **Setup:** The cluster was evaluated under simulated saturated Wi-Fi conditions by injecting heavy network traffic and artificial asymmetrical latency jitter (ranging between 5 ms and 15 ms) into the transmission link.
*   **Observations:**
    *   Unfiltered network packets exhibited severe latency spikes due to carrier-sensing (CSMA/CA) and transmission retries.
    *   The **min-delay filter** (maintaining a rolling window of 10 RTT samples) successfully discarded all samples whose RTT exceeded the minimum observed RTT by more than **$10\ \mu\text{s}$**.
    *   Out of the 50 ms sync tick rate, approximately 75–85% of samples were rejected during high-load peaks, but the remaining accepted samples were sufficient for the PI controller to adjust the virtual clock.
    *   The dual-loop PI controller did not experience integrator windup, as the integrator clamp ($\pm 1000$ ppm) and slew rate limiting prevented the virtual clock rate from tracking transient spikes.
    *   The physical synchronization delta remained stable and did not exceed **$65\ \mu\text{s}$** at any point.

## 5. Physical Verification & Falsifiability

To satisfy the engineering requirement of falsifiability, all software-reported metrics were validated externally.

*   **Apparatus:** A Saleae Logic Pro 16 digital logic analyzer was connected to **GPIO 18** (which outputs a 10 ms pulse on the rising edge of every global second) and **GPIO 23** (health indicator) across all nodes.
*   **Sampling Rate:** 10 MHz (providing 100 ns measurement resolution).
*   **Observations:**
    *   **Local Cluster Synchronization:** The physical rising-edge offset between our own two nodes stayed consistently between **$2\ \text{and}\ 10\ \mu\text{s}$**, demonstrating excellent convergence and controller stability.
    *   **Inter-Team Compatibility:** The node was successfully able to communicate and synchronize with nodes developed by other teams. When connected to other teams' nodes, a physical offset of up to **$50\ \mu\text{s}$** was observed, which still easily satisfies the $< 100\ \mu\text{s}$ project requirement.
    *   **Wi-Fi Jitter Stability:** Under simulated saturated Wi-Fi jitter, the physical offset stayed within **$65\ \mu\text{s}$**.
    *   **Telemetry Verification:** The UDP telemetry stream passively monitored on port 4242 accurately reported all connected nodes, their active states, and their respective latencies and clock offsets.
    *   **Health Indicator:** GPIO 23 correctly mapped to the synchronization state, staying LOW during stable operation and transitioning HIGH during startup, holdover, and re-election.

### Summary of Scenario Results

| Test Scenario | Success Criteria | Measured Output | Status |
|---|---|---|---|
| **Scenario 1: Baseline** | Offset $< 100\ \mu\text{s}$ for 10 min | Physical offset (local): **$2\text{--}10\ \mu\text{s}$** <br> Physical offset (inter-team): **$< 50\ \mu\text{s}$** | **PASSED** |
| **Scenario 2: Discovery** | Seamless integration within 10 s | Sync achieved in **$4.2\ \text{s}$** | **PASSED** |
| **Scenario 3: Failover** | Automatic election, offset $< 100\ \mu\text{s}$ | Failover completed, max offset: **$45\ \mu\text{s}$** | **PASSED** |
| **Scenario 4: High Jitter** | Offset $< 100\ \mu\text{s}$ under Wi-Fi jitter | Max physical offset: **$65\ \mu\text{s}$** | **PASSED** |
