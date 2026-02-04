# Testing Guide - Current Implementation

This guide will help you test what's already implemented in the MediaStream project.

## 🎯 What's Currently Working

- ✅ TorrentEngine (add/remove/status torrents)
- ✅ REST API endpoints (add torrent, get status, remove torrent)
- ✅ Qt Client UI (basic interface)
- ✅ Database schema (ready but not yet integrated)
- ✅ Content discovery services (ready but not yet integrated)

## 📋 Prerequisites Check

### Ubuntu Server
```bash
# Check if you have the required tools
gcc --version        # Should be 10+
cmake --version      # Should be 3.20+
python3 --version    # Should be 3.8+
conan --version      # Should be 2.0+ (install if missing: pip3 install conan)
```

### Windows Client
```powershell
# Check if you have Visual Studio
# Open "Developer Command Prompt for VS 2019" or "VS 2022"
cl          # Should show Microsoft C/C++ compiler

# Check CMake
cmake --version     # Should be 3.10+
```

---

## 🖥️ PART 1: Build & Run Server (Ubuntu)

### Step 1: Install Conan (if not installed)

```bash
pip3 install conan
conan profile detect
```

### Step 2: Navigate to Server Directory

```bash
cd ~/mediaStream/server
```

### Step 3: Install Dependencies

```bash
mkdir build
cd build

# Install all dependencies via Conan
conan install .. --build=missing -s compiler.cppstd=20

# This will download and build:
# - libtorrent
# - spdlog
# - fmt
# - oatpp
# - oatpp-sqlite
# - libcurl
# - nlohmann_json
# Takes 10-30 minutes on first run
```

**Note**: If you get errors about missing packages, try:
```bash
conan install .. --build=missing -s compiler.cppstd=20 -s build_type=Release
```

### Step 4: Build Server

```bash
# Configure CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# Build (use all CPU cores)
cmake --build . -j$(nproc)
```

**Expected output**: `mediastream_server` executable in `build/` directory

### Step 5: Run Server

```bash
# Create downloads directory
mkdir -p downloads

# Run server
./mediastream_server
```

**Expected output**:
```
[HH:MM:SS] [info] MediaStream Server V2.0 - Starting up...
[HH:MM:SS] [info] TorrentEngine initialized. Path: ./downloads
[HH:MM:SS] [info] Booting Core...
[HH:MM:SS] [info] Booting API...
[HH:MM:SS] [info] HttpServer started on background thread.
[HH:MM:SS] [info] REST API listening on port 8000...
[HH:MM:SS] [info] System Online.
```

### Step 6: Test Server API (from Ubuntu)

Open a new terminal and test the API:

```bash
# Test 1: Check server status
curl http://localhost:8000/api/v1/status

# Expected: {"status_code":200,"message":"OK"} or empty array []

# Test 2: Add a test magnet (Ubuntu torrent as example)
curl -X POST http://localhost:8000/api/v1/torrents \
  -H "Content-Type: application/json" \
  -d '{"magnet_link":"magnet:?xt=urn:btih:3b245504cf5f11bbdbe1201cea6a6bf45aee1bc0&dn=ubuntu-22.04.3-desktop-amd64.iso"}'

# Expected: {"status_code":200,"message":"Torrent added successfully"}

# Test 3: Check status again
curl http://localhost:8000/api/v1/status

# Expected: JSON array with torrent info including progress, state, etc.
```

### Step 7: Find Your Server IP

```bash
# Get your Ubuntu server's IP address
ip addr show | grep "inet 192.168"

# Example output: inet 192.168.1.37/24
# Your server IP is: 192.168.1.37
```

**Write down this IP - you'll need it for the client!**

### Step 8: Test from Windows PC

From your Windows 10 PC, open PowerShell:

```powershell
# Replace 192.168.1.37 with YOUR server IP
curl http://192.168.1.37:8000/api/v1/status

# Or open browser and navigate to:
# http://192.168.1.37:8000/api/v1/status
```

If this works, your server is ready! ✅

---

## 💻 PART 2: Build & Run Client (Windows 10)

### Step 1: Install Qt (if not installed)

**Option A: Qt Installer (Recommended)**
1. Download: https://www.qt.io/download-qt-installer
2. Install Qt 5.15.2
3. Select: MSVC 2019 64-bit, Qt Multimedia, Qt Network
4. Default path: `C:\Qt\5.15.2\msvc2019_64`

**Option B: vcpkg**
```powershell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install qt5-base:x64-windows qt5-multimedia:x64-windows
```

### Step 2: Open Developer Command Prompt

- Search for "x64 Native Tools Command Prompt for VS 2019"
- Or "Developer PowerShell for VS 2019"
- Run as Administrator

### Step 3: Navigate to Client Directory

```powershell
cd C:\Users\YourName\Desktop\Cosas\miscosas\mediaStream\client
```

### Step 4: Update Server IP in Code

Edit `mainwindow.h` and change the server IP:

```cpp
// Line 51-52 in mainwindow.h
QString serverIp = "192.168.1.37";  // YOUR Ubuntu server IP here
int serverPort = 8000;
```

### Step 5: Build Client

```powershell
mkdir build
cd build

# Configure (adjust Qt path if different)
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64

# Build
cmake --build . --config Release
```

**Expected output**: `client.exe` in `build\Release\` directory

### Step 6: Deploy Qt DLLs

```powershell
cd Release

# Run windeployqt to copy Qt DLLs
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe client.exe
```

This copies all required Qt DLLs to the Release folder.

### Step 7: Run Client

```powershell
# Still in build\Release directory
.\client.exe
```

**Expected**: Qt window opens with dark theme showing "MediaStream: Available Movies"

---

## 🧪 PART 3: Test Current Functionality

### Test 1: Server Status Check

1. In the client window, click **"Check Server Status"** button
2. Look at the log area at the bottom
3. Should show: "Servidor: [torrent status data]"

### Test 2: Add Torrent via Client

Currently, the client has a commented-out magnet input. Let's test via API:

**From Windows PowerShell:**
```powershell
# Add a small test torrent (replace with your server IP)
curl -X POST http://192.168.1.37:8000/api/v1/torrents `
  -H "Content-Type: application/json" `
  -d '{\"magnet_link\":\"magnet:?xt=urn:btih:3b245504cf5f11bbdbe1201cea6a6bf45aee1bc0\"}'
```

**From Ubuntu terminal:**
```bash
curl -X POST http://localhost:8000/api/v1/torrents \
  -H "Content-Type: application/json" \
  -d '{"magnet_link":"magnet:?xt=urn:btih:3b245504cf5f11bbdbe1201cea6a6bf45aee1bc0"}'
```

### Test 3: Monitor Download Progress

**From Ubuntu:**
```bash
# Watch status updates every 2 seconds
watch -n 2 'curl -s http://localhost:8000/api/v1/status | jq'

# Or without jq:
watch -n 2 'curl -s http://localhost:8000/api/v1/status'
```

You should see:
- `progress`: increasing from 0.0 to 1.0
- `state`: "Downloading" → "Finished"
- `download_rate`: bytes per second

### Test 4: Check Downloaded Files

**On Ubuntu server:**
```bash
cd ~/mediaStream/server/build/downloads
ls -lh

# You should see the downloaded torrent files
```

### Test 5: Remove Torrent

```bash
# Get the info_hash from status response
curl http://localhost:8000/api/v1/status

# Copy the "info_hash" value, then:
curl -X DELETE http://localhost:8000/api/v1/torrents/{INFO_HASH_HERE}

# Example:
curl -X DELETE http://localhost:8000/api/v1/torrents/3b245504cf5f11bbdbe1201cea6a6bf45aee1bc0
```

---

## 🔍 Troubleshooting

### Server Issues

**Problem**: "Port 8000 already in use"
```bash
# Find what's using port 8000
sudo netstat -tulpn | grep 8000

# Kill the process
sudo kill -9 <PID>
```

**Problem**: "Permission denied" on downloads folder
```bash
chmod 755 ~/mediaStream/server/build/downloads
```

**Problem**: Conan dependencies fail
```bash
# Clear Conan cache and retry
conan remove "*" -c
conan install .. --build=missing -s compiler.cppstd=20
```

### Client Issues

**Problem**: "Qt5Core.dll not found"
```powershell
# Run windeployqt again
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe client.exe
```

**Problem**: Client can't connect to server
1. Check server IP is correct in `mainwindow.h`
2. Verify server is running: `curl http://SERVER_IP:8000/api/v1/status`
3. Check Windows firewall isn't blocking
4. Ping server: `ping 192.168.1.37`

**Problem**: Build errors
```powershell
# Clean and rebuild
rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64
cmake --build . --config Release
```

---

## 📊 What to Expect

### Current Limitations

1. **No movie discovery yet**: The YTS/EZTV/Nyaa clients are implemented but not integrated into the API
2. **No streaming yet**: You can download torrents but can't stream them yet
3. **Basic UI**: Client shows interface but movie list is not populated
4. **No database integration**: Database code exists but isn't connected to the API yet

### What Works

1. ✅ Server starts and listens on port 8000
2. ✅ REST API accepts torrent magnet links
3. ✅ TorrentEngine downloads torrents sequentially
4. ✅ Status endpoint shows download progress
5. ✅ Client connects to server
6. ✅ Cross-platform: Ubuntu server + Windows client

---

## 🎯 Next Steps After Testing

Once you confirm everything works:

1. **Integrate content discovery**: Connect YTS/EZTV/Nyaa to API endpoints
2. **Add streaming endpoint**: Implement HTTP Range requests
3. **Enhance client UI**: Show discovered movies, add video player
4. **Connect database**: Store media metadata and watch history

---

## 📝 Quick Test Checklist

- [ ] Server builds successfully on Ubuntu
- [ ] Server starts without errors
- [ ] Server responds to `curl http://localhost:8000/api/v1/status`
- [ ] Server accessible from Windows: `curl http://SERVER_IP:8000/api/v1/status`
- [ ] Client builds successfully on Windows
- [ ] Client.exe runs and shows UI
- [ ] Can add torrent via API
- [ ] Can see download progress
- [ ] Downloaded files appear in `downloads/` folder
- [ ] Can remove torrent via API

If all checkboxes are ✅, you're ready to continue development!