# Setup Guide: Running on Two Pi Nodes

This guide explains how to get the DRS high-precision clock synchronization system running on exactly two Raspberry Pi 4B nodes.

---

## 1. Network Configuration

The nodes must communicate via wired Gigabit Ethernet to meet the low-jitter timing constraints.

1. Connect both Raspberry Pis directly using an Ethernet cable (or via a dedicated Gigabit switch).
2. Configure static IPs on both nodes using `systemd-networkd`. Check the interface name using `ip link show` (usually `eth0` or `end0`).
3. Create `/etc/systemd/network/eth0.network` (or similar interface name) on both Pis:

**Node 1 (Leader):**
```ini
[Match]
Name=eth0

[Network]
Address=10.0.0.11/24
```

**Node 2 (Follower):**
```ini
[Match]
Name=eth0

[Network]
Address=10.0.0.12/24
```

4. Restart the network service on both Pis:
```bash
sudo systemctl disable --now NetworkManager
sudo systemctl enable --now systemd-networkd
sudo systemctl restart systemd-networkd
```

---

## 2. Operating System & Hardening

To achieve sub-100 µs precision, the operating system must run a `PREEMPT_RT` kernel, and the synchronization thread must execute on a dedicated CPU core.

### 2.1 CPU Core Isolation
Isolate Core 3 from the Linux scheduler. Add the following parameters to the single line in `/boot/firmware/cmdline.txt` (do not add any newline):
```text
isolcpus=3 nohz_full=3 rcu_nocbs=3
```

### 2.2 Disable Competing Time Services
Disable any services that might adjust or interfere with the host system clock:
```bash
sudo systemctl disable --now systemd-timesyncd chronyd ntpd ptp4l phc2sys
```

### 2.3 Set CPU Governor to Performance
Set the CPU frequency scaling governor to `performance` persistently. Create `/etc/systemd/system/cpu-performance.service`:
```ini
[Unit]
Description=Set CPU governor to performance

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```
Enable and start the service:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now cpu-performance
```

Reboot both nodes to apply kernel configurations.

---

## 3. Build & Deploy

Compile the binary on your development host (Windows + WSL2) and deploy it to both Pis.

### 3.1 Cross-Compile (WSL2)
Run the following commands in WSL2 to cross-compile the binary for aarch64 ARM:
```bash
wsl cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchain-aarch64.cmake
wsl cmake --build build-arm
```

### 3.2 Deploy to Nodes
Deploy the built binary to the Pis. This script copies the executable to `/usr/local/bin/drs_sync` on the target Pi and restarts the systemd service.

```bash
# Deploy to Node 1 (Leader)
wsl bash scripts/deploy.sh 10.0.0.11

# Deploy to Node 2 (Follower)
wsl bash scripts/deploy.sh 10.0.0.12
```

---

## 4. Running the Synchronization

The node with the lowest numeric ID automatically becomes the cluster leader. 

### 4.1 Running Manually
To test and inspect outputs manually in real-time, SSH into each node and run:

*   **Node 1 (Leader, ID 1):**
    ```bash
    sudo /usr/local/bin/drs_sync 1 10.0.0.1
    ```
    *(Here, `10.0.0.1` represents your development machine IP where the telemetry listener runs).*
    
*   **Node 2 (Follower, ID 2):**
    ```bash
    sudo /usr/local/bin/drs_sync 2 10.0.0.1
    ```

### 4.2 Running as a Service
To run persistently, enable and start the systemd service unit `/etc/systemd/system/drs_sync.service`:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now drs_sync
```

---

## 5. Verification

Verify that the system is operating under real-time constraints:

1.  **Memory Locked:** Ensure memory is locked into RAM to avoid page-fault latency spikes:
    ```bash
    grep VmLck /proc/$(pgrep drs_sync)/status
    # Output should show a non-zero value (e.g., VmLck: 8192 kB)
    ```
2.  **Isolated Core Execution:** Verify the process runs on CPU 3:
    ```bash
    grep ^processor /proc/$(pgrep drs_sync)/task/*/stat
    # The last numeric column in the output should display 3
    ```
3.  **Synchronization Health:** 
    *   Observe **GPIO 23** (health indicator) on Node 2. It will boot HIGH, and then transition to **LOW** once the virtual clock offset has stayed below 100 µs for 10 continuous seconds.
    *   Connect **GPIO 18** from both nodes to a logic analyzer. Measure the offset between the rising edges of the 10 ms pulses (which fire once per second). They should be aligned well within the `< 100 µs` budget.
