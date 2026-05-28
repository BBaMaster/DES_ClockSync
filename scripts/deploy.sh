#!/usr/bin/env bash
set -euo pipefail

RPI_IP="${1:?usage: deploy.sh <rpi-ip> [node-id] [telem-dest-ip]}"
NODE_ID="${2:-1}"
TELEM_IP="${3:-}"
BINARY="build-arm/drs_sync"
REMOTE_BIN="/usr/local/bin/drs_sync"
SERVICE="drs_sync"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run the ARM cross-compile first." >&2
    exit 1
fi

echo "==> Uploading $BINARY to $RPI_IP:$REMOTE_BIN"
scp "$BINARY" "user@${RPI_IP}:/tmp/drs_sync"

# Build ExecStart line with optional telemetry destination
EXEC_START="/usr/local/bin/drs_sync ${NODE_ID}"
if [ -n "$TELEM_IP" ]; then
    EXEC_START="${EXEC_START} ${TELEM_IP}"
fi

echo "==> Writing service file (ExecStart: $EXEC_START)"
TMPFILE=$(mktemp)
cat > "$TMPFILE" << EOF
[Unit]
Description=DRS Synchronization Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStartPre=-/sbin/ip route add 224.0.0.0/4 dev eth0
ExecStart=${EXEC_START}
Restart=on-failure
RestartSec=5s
CPUAffinity=3
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=85
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
EOF
scp "$TMPFILE" "user@${RPI_IP}:/tmp/${SERVICE}.service"
rm "$TMPFILE"
ssh -t "user@${RPI_IP}" "sudo mv /tmp/${SERVICE}.service /etc/systemd/system/${SERVICE}.service"

echo "==> Installing and restarting $SERVICE on $RPI_IP"
ssh -t "user@${RPI_IP}" "sudo systemctl daemon-reload && sudo mv /tmp/drs_sync $REMOTE_BIN && sudo chmod +x $REMOTE_BIN && sudo systemctl restart $SERVICE && sleep 2 && sudo systemctl status $SERVICE --no-pager"
