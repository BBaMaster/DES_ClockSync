# Visualizer Integration & Protocol Corrections

This chapter documents the integration of the cluster visualizer and details a key correction made to the network packet layout to match the protocol specification.

---

## Visualizer Submodule Integration

To monitor the real-time status of the synchronization cluster without introducing the *probe effect* (i.e. debugging logs or print statements that would disrupt the microsecond-level timing accuracy of the synchronization loop), the project includes a real-time web-based visualizer.

### Repository Integration
The visualizer is integrated into the project as a Git submodule under the path `DRS-Cluster-Visualizer/` pointing to the repository:
[DRS-Cluster-Visualizer Submodule](https://github.com/jakobgif/DRS-Cluster-Visualizer)

### Architectural Design
The visualizer operates entirely out-of-band and passively. It does not send packets to the cluster or interfere with node operations. Instead, it sniffs the network multicast traffic.

It is split into two components:
1.  **Backend (Node.js Server):** 
    *   Runs on port `3001`.
    *   Creates a UDP socket, sets the socket options to reuse the port, and joins the DRS multicast group `239.192.88.100` on port `47200`.
    *   Listens for incoming binary packets, extracts the raw fields (Node ID, State, offset, RTT, sequence number, and timestamps), and reformats them.
    *   Establishes a WebSocket server to broadcast these parsed updates in real-time to all connected web clients.
2.  **Frontend (React Web Application):**
    *   Runs on port `5173` (built with Vite).
    *   Connects to the backend WebSocket server.
    *   Renders a dynamic dashboard visualizing the network activity.

### Features Offered
*   **Cluster Graph:** A circular topology diagram showing all detected nodes in the cluster. Nodes are color-coded by their state:
    *   **Gold:** `LEADER` (Authoritative clock source)
    *   **Blue:** `FOLLOWER` (Synchronized to leader)
    *   **Orange:** `HOLDOVER` (Temporary freerun mode)
    *   **Red:** `FAULT` (Synchronization failure)
    *   **Cyan:** `CALIBRATION` (Self-latency calibration in progress)
    *   **Gray:** `GROUND` (Startup stabilization)
    *   Animated lines connect the nodes, showing live transmission flow of `ANNOUNCE`, `SYNC_REQ`, and `SYNC_RESP` messages.
*   **Node Cards:** Detailed individual status windows displaying the Node ID, active state, calculated offset in microseconds, measured RTT, sequence numbers, election term, and synchronization flags.
*   **Packet Log:** A scrolling terminal-like log that tracks every parsed UDP packet showing source IP, timestamp, sequence number, message type, and CRC validation status.

---

## Protocol Packet Length Correction

During the implementation phase, an arithmetic discrepancy was identified in the wire protocol specification of the Architecture Anchor document (`drs_architecture_reviewed_fixed_v_21.md`):

*   **Section 4.2** specifies: *"All packets SHALL be exactly 64 bytes."*
*   **Section 4.3** outlines the binary packet layout:
    *   Header and timestamp fields sum up to **50 bytes** (Magic through T4).
    *   CRC32 takes **4 bytes** (Offsets 50–53).
    *   Padding is listed as `Padding: uint8[12] | 12` (12 bytes).
    *   **The Discrepancy:** Summing these fields ($50 + 4 + 12 = 66$ bytes) results in a packet size of **66 bytes**, which violates the 64-byte constraint.

### Code Adjustment for Compatibility
To ensure strict compatibility and enable our nodes to communicate seamlessly with other teams' hardware nodes operating on the shared network segment, the padding size was corrected in the source code.

*   In `src/protocol.h`, the packet size `DRS_PKT_SIZE` is defined as `64`.
*   The padding size was reduced from 12 bytes to **10 bytes** (`uint8[10]` at offsets 54–63).
*   The serialization and deserialization functions in `src/protocol.c` (`pkt_serialize` and `pkt_deserialize`) use the corrected 10-byte padding to pack and unpack the buffer, ensuring that the packet size on the wire is exactly **64 bytes**.

This adjustment successfully enabled inter-team node discovery and synchronization.
