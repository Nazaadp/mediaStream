# MediaStream - Private Netflix-like Streaming Platform

A local, private application to discover, download, and stream movies, TV series, and anime via torrents. Built with a high-performance C++ backend (oatpp/libtorrent) and a modern SvelteKit + Tauri desktop client.

## 🎯 Features

### Current Implementation

- ✅ **Content Discovery**: Automatic fetching from YTS (movies), EZTV (series), and Nyaa (anime) enriched with TMDB metadata.
- ✅ **SvelteKit/Tauri Desktop Client**: A modern, responsive, Netflix-style dark theme interface.
- ✅ **Database Management**: SQLite-based storage for media, torrents, and watch history.
- ✅ **Torrent Engine**: Sequential download optimized for instant streaming.
- ✅ **Direct Video Streaming**: HTTP Range integration for seamless HTML5 `<video>` playback.
- ✅ **Offline Caching**: Aggressive localStorage/sessionStorage usage for instant UI response and lower server loads.
- ✅ **WebSocket Syncing**: Real-time download progress broadcasting.

### Planned Features

- 🚧 **Search & Filter**: Refined discovery across all sources.
- 📋 **Authentication**: Multi-user account handling.
- 📋 **Subtitles**: OpenSubtitles integration.

## 🏗️ Architecture

```text
┌─────────────────┐
│  Tauri Client   │  Browse, search, discover TMDB
│  (SvelteKit)    │  HTML5 Video streaming
└────────┬────────┘
         │ HTTPS / WebSockets
┌────────▼────────┐
│  oatpp Server   │  REST API, File Serving
│  (C++/oatpp)    │  Internal HTTP Port: 8000
│                 │  Local Network: HTTPS 443
└────────┬────────┘
         │
┌────────▼────────┐
│  libtorrent     │  Sequential torrent downloads
│  + SQLite       │  Database (History, Media)
└─────────────────┘
```

The UI is built with **Svelte 5** and **Tauri v2**. The backend uses **C++20** and **Oat++** to handle content discovery, direct-to-memory media manipulation via **libtorrent**, and API service on internal port 8000 (exposed to the network via HTTPS on port 443).

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed system design.

## 📋 Prerequisites

### Server (Ubuntu/Linux)

- C++20 compiler (GCC 10+, Clang 12+)
- CMake 3.20+
- Conan 2.0+
- Python 3.8+ (for Conan)

### Client (Windows/macOS/Linux)

- Node.js (v18+)
- npm or pnpm
- Rust and Cargo (for Tauri build pipeline)

## 🚀 Quick Start

### 1. Build & Run Server

The server handles downloading and database management. Ensure you have an appropriate `.env` file containing `TMDB_APIKEY_AT`.

```bash
cd server
mkdir build && cd build

# Install dependencies with Conan
conan install .. --build=missing -s compiler.cppstd=20

# Configure and Build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run
./mediastream_server
```

The oat++ server will start internally on `http://0.0.0.0:8000`. Note this system expects a reverse proxy/firewall to expose the API securely to the rest of the LAN via TLS/HTTPS on port `443`.

### 2. Build & Run Client (Tauri Desktop)

Ensure you have Rust and Node.js installed.

```bash
cd clientTauri

# Install Node dependencies
npm install

# Run the Tauri development app
npm run tauri dev
```

## 📁 Project Structure

```text
mediaStream/
├── server/                      # C++ Backend Server (oatpp)
│   ├── include/mediastream/
│   │   ├── core/               # TorrentEngine (libtorrent)
│   │   ├── database/           # SQLite database layer (oatpp-sqlite)
│   │   └── api/                # REST API & WebSocket controllers
│   ├── src/
│   │   ├── core/
│   │   ├── database/
│   │   ├── api/
│   │   └── main.cpp
│   ├── CMakeLists.txt
│   └── conanfile.txt
│
├── clientTauri/                 # SvelteKit + Tauri Client
│   ├── src/
│   │   ├── lib/                # Reusable components (MediaCard)
│   │   └── routes/             # SvelteKit routing
│   ├── src-tauri/               # Rust OS-level bindings
│   ├── package.json
│   └── tauri.conf.json
│
├── ARCHITECTURE.md              # Detailed system design
├── WORKFLOW.md                  # Development practices and solved bugs
└── README.md                    # This file
```

## 📡 Core API Endpoints

- `GET /api/v1/stream/{infoHash}` - Stream video (HTTP Range support built-in)
- `POST /api/v1/torrents` - Add magnet link for downloading
- `DELETE /api/v1/torrents/{infoHash}` - Remove torrent logic
- `GET /api/v1/status` - Get active torrents via REST
- `POST /api/v1/user/history` - Client uploads complete TMDB JSON + watch progress
- `GET /api/v1/user/history` - Fetch user's watch history array
- `wss://{server-ip}:443/api/v1/ws/status` - Live torrent progress stream (Note: Routes through HTTPS proxy)
- `GET /api/v1/discover/*` - Content Discovery (Movies, Series, Anime) managed by the C++ backend

## 🔒 Security Considerations

- **Encryption**: Forced RC4 encryption for torrent traffic.
- **Non-root**: Server refuses to run as root user.
- **Input Validation**: Strict JSON payload casting and Magnet URI validation.
- **File Permissions**: Restrictive umask (0077) on downloads.
- **CORS**: Strict lockdown natively allowing only `http://localhost:1420`.

## 🐛 Troubleshooting

### Server won't start

- Check if the port (default 443/8000) is available.
- Ensure you're not running as root.

### Client can't connect (CORS or Fetch Failed)

- Ensure the server is running and accessible at the IP defined in the client's configuration (usually `.env` inside `clientTauri/`).
- Verify firewall settings on Ubuntu for port `443`/`8000`.

## 📄 License

Private use only. Not for distribution.

## 🤝 Contributing

For questions or suggestions, refer to `WORKFLOW.md` for architectural guidelines when contributing.

## ⚠️ Legal Disclaimer

This software is for educational and personal use only. Users are responsible for ensuring they have the legal right to download and stream any content. The developers do not condone piracy and are not responsible for how this software is used.
