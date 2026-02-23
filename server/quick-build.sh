#!/bin/bash

# MediaStream Server - Quick Build Script
# Usage: ./quick-build.sh

set -e  # Exit on error

echo "========================================="
echo "MediaStream Server - Quick Build"
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

# Check dependencies
echo "[1/6] Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: cmake not found${NC}"
    echo "Install: sudo apt update && sudo apt install cmake"
    exit 1
fi

if ! command -v make &> /dev/null || ! command -v g++ &> /dev/null; then
    echo -e "${RED}ERROR: Build tools not found (make, g++)${NC}"
    echo "Install: sudo apt update && sudo apt install build-essential"
    exit 1
fi

if ! command -v conan &> /dev/null; then
    echo -e "${RED}ERROR: conan not found${NC}"
    echo -e "${YELLOW}Ubuntu 24.04 blocks global pip installs. You must use pipx.${NC}"
    echo "Run the following commands to install Conan securely:"
    echo "  sudo apt install pipx"
    echo "  pipx ensurepath"
    echo "  pipx install conan"
    echo "Then, restart your SSH session and run this build script again."
    exit 1
fi

echo -e "${GREEN}✓ Dependencies OK${NC}"

# Clean previous build
echo ""
echo "[2/6] Cleaning previous build..."
if [ -d "build" ]; then
    rm -rf build
fi
mkdir build
cd build

# Install Conan dependencies
echo ""
echo "[3/6] Installing dependencies (this may take 10-30 minutes on first run)..."
conan install .. --build=missing -s compiler.cppstd=20 -s build_type=Release

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: Conan install failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Dependencies installed${NC}"

# Configure CMake
echo ""
echo "[4/6] Configuring CMake..."

TOOLCHAIN=$(find . -name conan_toolchain.cmake | head -n 1)
if [ -z "$TOOLCHAIN" ]; then
    echo -e "${RED}ERROR: conan_toolchain.cmake not found. Conan might have failed silently.${NC}"
    exit 1
fi

echo "Using toolchain: $TOOLCHAIN"
cmake .. -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: CMake configuration failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ CMake configured${NC}"

# Build
echo ""
echo "[5/6] Building (using $(nproc) cores)..."
cmake --build . -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build successful${NC}"

# Create downloads directory
echo ""
echo "[6/6] Setting up directories..."
mkdir -p downloads
chmod 755 downloads

echo -e "${GREEN}✓ Setup complete${NC}"

# Get server IP
echo ""
echo "========================================="
echo -e "${GREEN}Build Complete!${NC}"
echo "========================================="
echo ""
echo "Server executable: $(pwd)/mediastream_server"
echo "Downloads folder: $(pwd)/downloads"
echo ""
echo "Your server IP addresses:"
ip addr show | grep "inet " | grep -v "127.0.0.1" | awk '{print "  - " $2}' | sed 's/\/.*$//'
echo ""
echo "To start server:"
echo "  cd build"
echo "  ./mediastream_server"
echo ""
echo "To test API:"
echo "  curl 127.0.0.1:8000/api/v1/status"
echo ""