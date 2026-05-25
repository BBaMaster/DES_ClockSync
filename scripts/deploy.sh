#!/usr/bin/env bash
set -euo pipefail

RPI_IP="${1:?usage: deploy.sh <rpi-ip>}"
BINARY="build-arm/drs_sync"
REMOTE_BIN="/usr/local/bin/drs_sync"
SERVICE="drs_sync"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run the ARM cross-compile first." >&2
    exit 1
fi

echo "==> Uploading $BINARY to $RPI_IP:$REMOTE_BIN"
scp "$BINARY" "pi@${RPI_IP}:${REMOTE_BIN}"

echo "==> Restarting $SERVICE on $RPI_IP"
ssh "pi@${RPI_IP}" "sudo chmod +x $REMOTE_BIN && sudo systemctl restart $SERVICE && sudo systemctl status $SERVICE --no-pager"
