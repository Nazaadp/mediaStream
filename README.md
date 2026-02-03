# MediaStream - Private Netflix-like Torrent Streaming Platform

A local, private application to discover, download, and stream movies, TV series, and anime via torrents. Built entirely in C++ with a Qt desktop client and oatpp server.

## 🎯 Features

### Current Implementation
- ✅ **Content Discovery**: Automatic fetching from YTS (movies), EZTV (series), and Nyaa (anime)
- ✅ **Database Management**: SQLite-based storage for media, torrents, and watch history
- ✅ **Torrent Engine**: Sequential download optimized for streaming
- ✅ **REST API**: oatpp-based HTTP server
- ✅ **Qt Desktop Client**: Netflix-style dark theme interface

### Planned Features
- 🚧 **TMDB Integration**: Rich metadata with posters, descriptions, and ratings
- 🚧 **HTTP Range Streaming**: Progressive video playback while downloading
- 📋 **Watch History**: Resume playback from where you left off
- 📋 **Search & Filter**: Find content across all sources
- 📋 **HTTPS Support**: Secure client-server communication
- 📋 **Video Player**: Integrated Qt Multimedia player

## 🏗️ Architecture

```
┌─────────────────┐
│  Qt Client      │  Browse, search, stream
│  (C++/Qt5)      │
└────────┬────────┘
         │ HTTPS REST API
┌────────▼────────┐
│  oatpp Server   │  Content discovery, torrent management
│  (C++/oatpp)    │
└────────┬────────┘
         │
┌────────▼────────┐
│  libtorrent     │  Sequential torrent downloads
│  + SQLite       │  Media database
└─────────────────┘
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed system design.

## 📋 Prerequisites

### Server
- C++20 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- CMake 3.20+
- Conan 2.0+
- Python 3.8+ (for Conan)

### Client
- C++17 compiler
- CMake 3.10+
- Qt5 (Widgets, Network, Multimedia)
- vcpkg (Windows) or system Qt packages (Linux/macOS)

## 🚀 Quick Start

### 1. Install Dependencies

#### Server (using Conan)
```bash
# Install Conan
pip install conan

# Create default profile
conan profile detect
```

#### Client (using vcpkg on Windows)
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # or bootstrap-vcpkg.bat on Windows

# Install Qt5
./vcpkg install qt5-base qt5-multimedia
```

#### Client (using system packages on Linux)
```bash
# Ubuntu/Debian
sudo apt install qt5-default qtmultimedia5-dev

# Fedora
sudo dnf install qt5-qtbase-devel qt5-qtmultimedia-devel

# Arch
sudo pacman -S qt5-base qt5-multimedia
```

### 2. Build Server

```bash
cd server
mkdir build && cd build

# Install dependencies with Conan
conan install .. --build=missing -s compiler.cppstd=20

# Build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run
./mediastream_server
```

The server will start on `http://0.0.0.0:8000`

### 3. Build Client

```bash
cd client
mkdir build && cd build

# Configure (adjust vcpkg path for Windows)
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release

# Run
./client  # or client.exe on Windows
```

## 📁 Project Structure

```
mediaStream/
├── server/                      # C++ Backend Server
│   ├── include/mediastream/
│   │   ├── core/               # TorrentEngine
│   │   ├── database/           # SQLite database layer
│   │   ├── services/           # Content discovery (YTS, EZTV, Nyaa)
│   │   └── api/                # REST API controllers
│   ├── src/
│   │   ├── core/
│   │   ├── database/
│   │   ├── services/
│   │   ├── api/
│   │   └── main.cpp
│   ├── CMakeLists.txt
│   └── conanfile.txt
│
├── client/                      # Qt Desktop Client
│   ├── main.cpp
│   ├── mainwindow.h
│   ├── mainwindow.cpp
│   └── CMakeLists.txt
│
├── ARCHITECTURE.md              # Detailed system design
└── README.md                    # This file
```

## 🔧 Configuration

### Server Configuration
Edit `server/src/main.cpp`:
```cpp
// Port
const int SERVER_PORT = 8000;

// Download directory
const std::string DOWNLOAD_DIR = "./downloads";

// Database path
const std::string DB_PATH = "./mediastream.db";
```

### Client Configuration
Edit `client/mainwindow.h`:
```cpp
QString serverIp = "192.168.1.37";  // Your server IP
int serverPort = 8000;
```

## 🎮 Usage

### Server
1. Start the server: `./mediastream_server`
2. Server listens on port 8000
3. Access API at `http://localhost:8000/api/v1/`

### Client
1. Launch the client application
2. Browse available content from YTS, EZTV, and Nyaa
3. Click on a movie/series to view details
4. Click "Download" to start torrent
5. Stream while downloading (once implemented)

## 📡 API Endpoints

### Current Endpoints
- `POST /api/v1/torrents` - Add magnet link
- `GET /api/v1/status` - Get active torrents
- `DELETE /api/v1/torrents/{hash}` - Remove torrent

### Planned Endpoints
- `GET /api/v1/discover/popular` - Popular content
- `GET /api/v1/discover/movies` - Movies from YTS
- `GET /api/v1/discover/series` - Series from EZTV
- `GET /api/v1/discover/anime` - Anime from Nyaa
- `GET /api/v1/search?q={query}` - Search all sources
- `GET /api/v1/stream/{media_id}` - Stream video (Range support)
- `POST /api/v1/watch-history` - Update watch progress

## 🛠️ Development Roadmap

### Phase 1: Foundation ✅
- [x] Database schema
- [x] Content discovery services
- [x] Basic torrent management
- [x] REST API structure

### Phase 2: Core Features 🚧
- [ ] TMDB metadata integration
- [ ] HTTP Range streaming endpoint
- [ ] Content manager orchestration
- [ ] Enhanced API endpoints

### Phase 3: Client Enhancement 📋
- [ ] Improved Qt UI
- [ ] Video player integration
- [ ] Watch history tracking
- [ ] Search and filtering

### Phase 4: Production Ready 📋
- [ ] SSL/TLS support
- [ ] Error handling and logging
- [ ] Performance optimization
- [ ] Documentation

## 🔒 Security Considerations

- **Encryption**: Forced RC4 encryption for torrent traffic
- **Non-root**: Server refuses to run as root user
- **Input Validation**: Magnet URI validation
- **File Permissions**: Restrictive umask (0077)
- **HTTPS**: Planned for client-server communication

## 🐛 Troubleshooting

### Server won't start
- Check if port 8000 is available: `netstat -tuln | grep 8000`
- Ensure you're not running as root
- Check logs for specific errors

### Client can't connect
- Verify server IP and port in client configuration
- Check firewall settings
- Ensure server is running: `curl http://localhost:8000/api/v1/status`

### Build errors
- Ensure all dependencies are installed
- Check C++ standard version (C++20 for server, C++17 for client)
- Update Conan/vcpkg packages

## 📝 Next Steps

To continue development, the next priorities are:

1. **TMDB Integration**: Fetch rich metadata for discovered content
2. **Streaming Endpoint**: Implement HTTP Range request handler
3. **Content Manager**: Orchestrate discovery, download, and metadata
4. **Client UI**: Enhance with video player and better navigation
5. **Testing**: Add unit tests and integration tests

## 📄 License

Private use only. Not for distribution.

## 🤝 Contributing

This is a private project. For questions or suggestions, contact the maintainer.

## ⚠️ Legal Disclaimer

This software is for educational and personal use only. Users are responsible for ensuring they have the legal right to download and stream any content. The developers do not condone piracy and are not responsible for how this software is used.