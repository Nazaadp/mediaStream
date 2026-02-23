#!/bin/bash

# VM for MediaStream Server - Quick Start Script
# Usage: ./quick-start-vm.sh

set -e  # Exit on error

echo "========================================="
echo "VM for MediaStream Server - Quick Start"
echo "========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
    echo -e "${RED}ERROR: Do not run as root/sudo${NC}"
    echo "The server refuses to run as root for security reasons"
    exit 1
fi

echo "Starting MediaStream Server VM..."

sudo virsh --connect qemu:///system start isolated-cpp-app

echo 

sudo virsh --connect qemu:///system console isolated-cpp-app

login:
appuser