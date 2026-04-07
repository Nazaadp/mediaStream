# MediaStream Project Context & State

## 📝 1. Project Description
MediaStream is a private, high-performance "Netflix-like" media discovery and streaming application. It is designed to run on a local Ubuntu server (virtualized via KVM) and serve content to Windows, Android, and Web clients with enterprise-grade security.

### Key Features:
- **Multi-Source Discovery**: Aggregates metadata from TMDB, Cinemeta, and Torrentio.
- **Sequential Streaming**: Uses `libtorrent` with sequential downloading to allow instantaneous playback of media as it downloads.
- **Security-First Architecture**: Zero-Trust network isolation, non-root execution, and reverse-proxied TLS.
- **Premium UI**: Svelte 5 + Tauri 2.0 desktop client with glassmorphism and keyboard-optimized navigation.

---

## 🚀 2. Current State (Phase: Search Refinement & Playback Stability)

### Recent Accomplishments:

#### 🔍 Global Search Implementation
- **Multi-Source Discovery**: Integrated TMDB Multi-Search and Cinemeta v3 as parallel discovery sources.
- **Categorization**: Results are dynamically split into **Movies**, **Series**, and **Anime** (detected via `original_language` and genre logic).
- **Localized Titles**: Enforced `en-US` locale on TMDB enrichment to guarantee consistent English titles across all media types.
- **Deduplication**: Robust merge logic combines duplicate hits from different discovery sources using TMDB/IMDB IDs as keys.
- **Keyboard Navigation**: Implemented arrow-key + Enter navigation across search result grids.

#### ⚡ The "Rattin-Temp" Optimization (Rating & Rate-Limiting)
Implemented to solve performance bottlenecks and search relevance issues:
- **Rate-Limiting (Temporal Control)**: Enforced a `500ms` delay between parallel Torrentio requests to prevent Cloudflare timeouts (RFC-compliant backoff).
- **Rating-Based Sorting**: Two-tier sorting algorithm:
  - **Primary**: Year (Descending) — ensures newest content appears first.
  - **Secondary**: Rating (Descending) — surfaces verified popular content within a given release year.
- **Result Truncation**: Capped raw search results to `20 items` per search to maintain sub-second UI responsiveness.

#### 🎥 Playback Bugfixes
- **WebSocket Status Sync**: Re-implemented the `run_ws_broadcaster` loop in `HttpServer.cpp` to reliably push `TorrentStatusDto` lists every second, fixing the "Fetching Metadata" UI hang.
- **MIME-Type Compatibility**: Updated `StreamController.hpp` to serve `.mkv` files with `video/webm` to leverage Chromium's native WebM decoding path.
- **Codec Diagnostic**: Added a 5-second no-video detection check in the watch page that prompts users to install **HEVC Video Extensions** or switch to H.264 content.
- **5% Progress Guard**: Maintained the pre-playback threshold; UI accurately transitions from "Fetching Metadata" → "Downloading" → "Playing".

---

## 🛠️ 3. Architecture

### Backend (C++ Server)
- **Language**: C++20
- **Framework**: Oat++ 1.3.0 (HTTP & WebSocket)
- **Torrent Engine**: libtorrent 2.0.10 (Sequential downloading)
- **Database**: SQLite via oatpp-sqlite
- **Enrichment**: TMDB (Metadata), Cinemeta v3 (Stremio Catalog), Torrentio (Multi-source Stremio Addon)
- **Infrastructure**: systemd daemon on KVM-virtualized Ubuntu 24.04, behind Nginx Reverse Proxy (TLS 443)

### Frontend (Desktop Client)
- **Language**: JavaScript / Svelte 5
- **Framework**: Tauri 2.0 (Rust shell)
- **Styling**: Vanilla CSS (glassmorphism, vibrant dark-mode palette)
- **Communication**: REST API + WebSockets for real-time torrent status

---

## 🛡️ 4. Security Posture
- **Zero-Trust Networking**: Backend strictly binds to `127.0.0.1`. All external access via Nginx Reverse Proxy with a self-signed 4096-bit RSA TLS certificate.
- **Sandboxing**: Server runs as `appuser` (non-root) with `NoNewPrivileges=yes`, `ProtectSystem=full`.
- **Binary Hardening**: All targets compiled with ASLR (`-fPIE`) and Stack Protection (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`).
- **Filesystem**: Downloads folder `chmod 750`, environment files `chmod 600`.

---

## 📈 5. Next Steps & Ongoing Objectives
- **Search UI Polish**: Complete keyboard navigation refinements for edge cases in search result grids.
- **Watch Progress Persistence**: Ensure watch state is accurately synced between the C++ SQLite DB and Svelte `localStorage` cache (dirty-flag strategy).
- **Advanced Filtering**: Add "Genre" and "Language" filters to the main discovery rows using the backend filter endpoints.
- **Resiliency**: Improve error handling for offline scenarios where TMDB, Cinemeta, or Torrentio may be unreachable (graceful degradation + cached results).
- **Subtitles**: Implement backend extraction or OpenSubtitles API integration.
- **Transcoding Fallback**: Evaluate lightweight `ffmpeg`-based remuxing for incompatible containers (`.mkv` → `.mp4`).
- **Mobile Client**: Optimize Svelte UI for Android deployment via Tauri Mobile.

---

## 🧪 6. Command Palette
```bash
# Backend: Rebuild & Restart
cd ~/cpp-app/server && ./quick-build.sh && sudo systemctl restart mediastream

# Backend: Live Logs
sudo journalctl -u mediastream -f

# Frontend: Dev Mode
cd clientTauri && npm run tauri dev

# Host: Start KVM VM
./start-cpp-vm.sh

# VM: Pull Latest Code
cd ~/cpp-app && git pull
```
