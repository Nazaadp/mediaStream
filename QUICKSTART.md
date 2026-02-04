# Quick Start Guide

## 🚀 Ubuntu Server (5 minutes)

```bash
# 1. Navigate to server directory
cd ~/mediaStream/server

# 2. Make build script executable
chmod +x quick-build.sh

# 3. Run build script (10-30 min first time, 2 min after)
./quick-build.sh

# 4. Start server
cd build
./mediastream_server

# 5. Get your server IP (write this down!)
ip addr show | grep "inet 192.168"
# Example: 192.168.1.37
```

**Test it works:**
```bash
curl http://localhost:8000/api/v1/status
```

---

## 💻 Windows 10 Client (5 minutes)

### First Time Setup

1. **Install Qt** (if not installed)
   - Download: https://www.qt.io/download-qt-installer
   - Install Qt 5.15.2 with MSVC 2019 64-bit

2. **Update Server IP**
   - Edit `client/mainwindow.h`
   - Line 51: Change `"192.168.1.37"` to YOUR server IP

3. **Build Client**
   ```powershell
   # Open "x64 Native Tools Command Prompt for VS 2019"
   cd C:\Users\YourName\Desktop\Cosas\miscosas\mediaStream\client
   .\build-windows.bat
   ```

4. **Run Client**
   ```powershell
   cd dist
   .\client.exe
   ```

---

## 🧪 Quick Test

### Test 1: Add Torrent (from Ubuntu)
```bash
curl -X POST http://localhost:8000/api/v1/torrents \
  -H "Content-Type: application/json" \
  -d '{"magnet_link":"magnet:?xt=urn:btih:3b245504cf5f11bbdbe1201cea6a6bf45aee1bc0&dn=ubuntu-22.04.3-desktop-amd64.iso"}'
```

### Test 2: Check Status
```bash
curl http://localhost:8000/api/v1/status
```

### Test 3: From Windows Client
- Click "Check Server Status" button
- Should show torrent info in log area

---

## 📚 Full Documentation

- **[TESTING_GUIDE.md](TESTING_GUIDE.md)** - Complete step-by-step testing
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System design
- **[WINDOWS_BUILD.md](WINDOWS_BUILD.md)** - Windows .exe creation
- **[README.md](README.md)** - Project overview

---

## 🐛 Common Issues

**Server won't start:**
```bash
# Check if port 8000 is in use
sudo netstat -tulpn | grep 8000
```

**Client can't connect:**
```powershell
# Test from Windows
curl http://YOUR_SERVER_IP:8000/api/v1/status
```

**Build errors:**
```bash
# Server: Clear Conan cache
conan remove "*" -c

# Client: Clean build
rmdir /s /q build
```

---

## ✅ What Works Now

- ✅ Server accepts torrent magnet links
- ✅ Downloads torrents sequentially
- ✅ Shows download progress via API
- ✅ Client connects to server
- ✅ Cross-platform (Ubuntu + Windows)

## 🚧 Coming Next

- Content discovery (YTS/EZTV/Nyaa integration)
- Video streaming with HTTP Range requests
- Enhanced client UI with movie posters
- Watch history tracking
- Search functionality

---

**Need help?** Check [TESTING_GUIDE.md](TESTING_GUIDE.md) for detailed troubleshooting!