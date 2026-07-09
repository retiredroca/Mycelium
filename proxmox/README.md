# Proxmox Deployment

## Quick Start

```bash
# Create a Mycelium LXC container (default ID: 200)
bash proxmox/create-mycelium-ct.sh

# Or specify container ID and hostname
bash proxmox/create-mycelium-ct.sh 201 my-node-1
```

## Requirements

- Proxmox VE 7.x or later
- `pct` and `pveam` commands available on the host
- Internet access for downloading the Ubuntu template and cloning the repository

## What the Script Does

1. Downloads the Ubuntu 22.04 LXC template (if not cached)
2. Creates a privileged container (4 GB disk, 512 MB RAM, 512 MB swap)
3. Installs build dependencies (git, cmake, build-essential, libssl-dev)
4. Clones and builds Mycelium inside the container
5. Installs the binary and configures a systemd service
6. Starts the node automatically with HTTP dashboard on port 8080

## Manual Management

```bash
# Enter container
pct enter <CT_ID>

# Check service status
pct exec <CT_ID> -- systemctl status mycelium

# View logs
pct exec <CT_ID> -- journalctl -u mycelium -f

# Restart node
pct exec <CT_ID> -- systemctl restart mycelium

# Stop container
pct stop <CT_ID>

# Destroy container
pct destroy <CT_ID>
```
