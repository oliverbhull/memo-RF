#!/bin/bash

# [DOC LINK]: https://docs.google.com/document/d/18TrYUjDtF99tVUm70zWMquitWSkQNhYwvfEm8E92FIs/edit?usp=sharing

# Exit immediately if a command fails
set -e

LOG_FILE="/var/log/netbird_setup.log"

echo "--- Starting NetBird Remote Access Setup ---" | sudo tee -a $LOG_FILE

# 1. Connectivity Guard
echo "Checking internet connectivity..."
if ! ping -c 1 -W 2 8.8.8.8 >/dev/null 2>&1; then
    echo "$(date): ERROR - No internet connection detected." | sudo tee -a $LOG_FILE
    exit 1
fi
echo "Internet connection verified."

# 2. Idempotent SSH Server Installation
if ! command -v sshd >/dev/null 2>&1; then
    echo "Installing OpenSSH Server..."
    sudo apt update && sudo apt install -y openssh-server
else
    echo "OpenSSH Server is already installed."
fi
sudo systemctl enable --now ssh

# 3. Idempotent NetBird Installation
if ! command -v netbird >/dev/null 2>&1; then
    echo "Installing NetBird client..."
    curl -fsSL https://pkgs.netbird.io/install.sh | sh
else
    echo "NetBird client is already installed."
fi

# 4. Infinite Auto-Recovery Configuration
echo "Configuring infinite auto-reconnect..."
sudo mkdir -p /etc/systemd/system/netbird.service.d
cat <<EOF | sudo tee /etc/systemd/system/netbird.service.d/override.conf
[Service]
Restart=always
RestartSec=10s
StartLimitIntervalSec=0
EOF

# Reload to pick up changes and restart to apply them
sudo systemctl daemon-reload
sudo systemctl restart netbird

# 5. Idempotent Connection Check
CURRENT_STATUS=$(netbird status 2>/dev/null || echo "Disconnected")

if echo "$CURRENT_STATUS" | grep -q "Connected"; then
    echo "NetBird is already connected. Skipping 'up' command."
else
    echo "Connecting to NetBird network..."
    sudo netbird up \
        --setup-key 7F6C0525-4812-4FD5-A7E2-36930A90BA2A \
        --allow-server-ssh \
        --hostname "$(hostname)"
fi

# 6. Capture IP and Log Results
NB_IP=$(netbird status | grep "NetBird IP" | awk '{print $3}' | cut -d'/' -f1)
echo "$(date): Setup successful. IP: $NB_IP" | sudo tee -a $LOG_FILE

echo "------------------------------------------"
echo "SETUP COMPLETE"
echo "Device Hostname: $(hostname)"
echo "NetBird IP:      $NB_IP"
echo "Log saved to:    $LOG_FILE"
echo "------------------------------------------"
echo ""
echo "Device accessible remotely via: https://app.netbird.io/peers"