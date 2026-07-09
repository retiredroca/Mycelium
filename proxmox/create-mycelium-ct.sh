#!/usr/bin/env bash
set -euo pipefail

# Proxmox Mycelium LXC Container Creator
# Usage: bash create-mycelium-ct.sh [CT_ID] [CT_HOSTNAME]
#
# Creates a privileged LXC container with Ubuntu 22.04, builds Mycelium,
# and configures it as a systemd service.

CT_ID="${1:-200}"
CT_HOSTNAME="${2:-mycelium-node}"
CT_STORAGE="local-lvm"
CT_TEMPLATE="ubuntu-22.04-standard_22.04-1_amd64.tar.zst"
CT_DISK="4"
CT_MEMORY="512"
CT_SWAP="512"
CT_NET_BRIDGE="vmbr0"
CT_IP="dhcp"

BUILD_DIR="/tmp/mycelium-build"
SERVICE_USER="mycelium"
SERVICE_DIR="/home/${SERVICE_USER}/mycelium"

echo "=== Creating Mycelium LXC container (ID: ${CT_ID}, Hostname: ${CT_HOSTNAME}) ==="

# Check prerequisites
command -v pct >/dev/null 2>&1 || { echo "ERROR: This script must be run on a Proxmox VE host (pct not found)"; exit 1; }

# Check if template exists
pveam list "${CT_STORAGE}" | grep -q "${CT_TEMPLATE}" || {
    echo "Downloading Ubuntu 22.04 template..."
    pveam download "${CT_STORAGE}" "${CT_TEMPLATE}"
}

# Create the container
pct create "${CT_ID}" "${CT_STORAGE}:vztmpl/${CT_TEMPLATE}" \
    --hostname "${CT_HOSTNAME}" \
    --storage "${CT_STORAGE}" \
    --rootfs "${CT_STORAGE}:${CT_DISK}" \
    --memory "${CT_MEMORY}" \
    --swap "${CT_SWAP}" \
    --net0 name=eth0,bridge="${CT_NET_BRIDGE}",ip="${CT_IP}" \
    --unprivileged 0 \
    --features keyctl=1,nesting=1 \
    --onboot 1

echo "=== Container created, starting ==="
pct start "${CT_ID}"

echo "=== Installing build dependencies ==="
pct exec "${CT_ID}" -- bash -c "
    apt-get update && apt-get install -y \
        git cmake build-essential libssl-dev ca-certificates \
        && rm -rf /var/lib/apt/lists/*
"

echo "=== Cloning and building Mycelium ==="
pct exec "${CT_ID}" -- bash -c "
    git clone https://github.com/retiredroca/Mycelium.git \"${BUILD_DIR}\"
    cd \"${BUILD_DIR}/mycelium-cpp\"
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
"

echo "=== Installing Mycelium binary ==="
pct exec "${CT_ID}" -- bash -c "
    adduser --disabled-password --gecos '' \"${SERVICE_USER}\"
    mkdir -p \"${SERVICE_DIR}\"
    cp \"${BUILD_DIR}/mycelium-cpp/build/mycelium\" \"/usr/local/bin/mycelium\"
    chown -R \"${SERVICE_USER}:${SERVICE_USER}\" \"${SERVICE_DIR}\"
"

echo "=== Configuring systemd service ==="
pct exec "${CT_ID}" -- bash -c "cat > /etc/systemd/system/mycelium.service <<'SERVICEEOF'
[Unit]
Description=Mycelium P2P Node
After=network.target

[Service]
Type=simple
User=${SERVICE_USER}
ExecStart=/usr/local/bin/mycelium start --http-port 8080 --listen /ip4/0.0.0.0/tcp/18028
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
SERVICEEOF"

pct exec "${CT_ID}" -- bash -c "
    systemctl daemon-reload
    systemctl enable mycelium
    systemctl start mycelium
"

echo "=== Cleaning up ==="
pct exec "${CT_ID}" -- rm -rf "${BUILD_DIR}"

echo "=== Done ==="
echo "Container ${CT_ID} (${CT_HOSTNAME}) is running."
echo "Web UI: http://$(pct exec ${CT_ID} -- hostname -I | awk '{print $1}'):8080"
echo "Attach: pct enter ${CT_ID}"
