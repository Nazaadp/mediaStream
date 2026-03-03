# MediaStream Architecture

## Overview

MediaStream is a private Netflix-like application that discovers, downloads, and streams torrents locally via HTTPS. Built with a high-performance C++ backend and a SvelteKit + Tauri desktop client.

## System Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│                   Tauri Desktop Client                       │
│  (Svelte 5 / Node.js / Rust)                                │
│  - Browse content catalog (TMDB, YTS, Nyaa via frontend)    │
│  - Optimistic UI caching (localStorage/sessionStorage)       │
│  - Stream HTML5 video via HTTP Range                         │
│  - Sync Watch History to Backend                             │
└────────────────────┬────────────────────────────────────────┘
                     │ HTTPS / WebSockets
                     │
┌────────────────────▼────────────────────────────────────────┐
│                  MediaStream Server                          │
│  (C++ / oatpp / libtorrent)                                 │
│  - Internal Port: 8000, External Proxy: HTTPS 443           │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              REST API Layer (oatpp)                   │  │
│  │  - Torrent management                                 │  │
│  │  - Video streaming (HTTP Range requests)              │  │
│  │  - Watch history tracking validation                  │  │
│  │  - WebSocket download progress broadcasters           │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            Core Services                              │  │
│  │  - TorrentEngine (libtorrent)                         │  │
│  │    * Sequential download for streaming                │  │
│  │    * Memory-managed chunk allocation                  │  │
│  │  - Database (SQLite via oatpp-sqlite)                 │  │
│  │    * Media metadata                                   │  │
│  │    * Torrent info                                     │  │
│  │    * Watch history / View Later                       │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Technology Stack

### Server (Headless)

- **Language**: C++20
- **HTTP Framework**: oatpp 1.3.0
- **Torrent Engine**: libtorrent-rasterbar 2.0.10
- **Database**: SQLite3 (via oatpp-sqlite)
- **JSON**: nlohmann_json 3.11.3
- **Logging**: spdlog
- **Networking/HTTP**: libcurl

### Client (Tauri Desktop App)

- **Framework**: SvelteKit (Svelte 5)
- **Desktop Wrapper**: Tauri v2
- **Video Player**: Native HTML5 `<video>`
- **Local State**: Browser Storage APIs (`localStorage`, `sessionStorage`)

## Database Schema

### media_items

Stores all discovered content (movies, series, anime). Acts as the single source of truth for TMDB/Imdb metadata.

- id, type, title, year, description
- poster_url, backdrop_url, rating, genres
- runtime_minutes, tmdb_id, imdb_id
- created_at, updated_at

### torrents

Stores torrent information linked to media.

- id, media_id, info_hash, magnet_uri
- quality, size_bytes, seeders, leechers
- source, status, progress, file_path
- created_at, updated_at

### watch_history

Tracks user viewing progress. Contains a Foreign Key (`UNIQUE(media_id)`) pointing to `media_items`.

- id, media_id, position_seconds
- duration_seconds, progress_percent
- last_watched, completed

### view_later

Stores favorites. Contains a Foreign Key (`UNIQUE(media_id)`) pointing to `media_items`.

## Content Discovery Flow (Backend Managed)

**The C++ Server natively integrates Content Discovery APIs** to supply data to the SvelteKit frontend without rate limits triggering on individual clients.

1. **Frontend Request**: The Tauri client requests movies/series natively from the server (`GET /api/v1/discover/movies`).
2. **C++ External Querying**: The `ContentDiscoveryManager` spins up `TMDBFetcher`, `YTSClient`, `NyaaClient`, and `EZTVClient` instances.
3. **Payload Formatting**: C++ parses the 3rd-party JSON internally and amalgamates the TMDB metadata alongside YTS/Nyaa Torrents into a single DTO.
4. **Delivery**: Svelte renders the aggregated DTO.
5. **Database Safekeeping**: When a stream starts or an item is saved to View Later natively, the frontend POSTs the *entire metadata payload* back to the C++ backend (`/api/v1/user/history`), avoiding subsequent round-trips to TMDB. The backend parses this JSON and upserts it into SQLite.

## Download & Streaming Flow

1. **Initiation**: A visual `MediaCard.svelte` fires a `▶ Stream` action.
2. **Torrent Binding**: Client issues `POST /api/v1/torrents` containing the torrent's Magnet URI.
3. **Routing**: Upon API success, SvelteKit programmatically routes to a dedicated player view at `/watch/[infoHash]`.
4. **Sequential Transport**: Server adds the magnet to libtorrent with sequential piece downloading to prioritize the start frames.
5. **Streaming Player**: The Svelte HTML5 `<video>` player routes an `HTTP Range` request to `GET /api/v1/stream/{infoHash}`.
6. **Chunk Delivery**: The backend reads the downloaded physical chunks and responds with `206 Partial Content`.
7. **WebSocket Syncing**: The `wss://{server-ip}:443/api/v1/ws/status` socket seamlessly updates the torrent progress.

## Streaming Protocol

**HTTP Range Requests** (RFC 7233)

- Client requests specific byte ranges (e.g., `bytes=0-4096`).
- Server validates the request against bounded memory arrays.
- Backed rigidly clamps chunks to **4 Megabytes** maximum to protect TCP windows.
- Responds with partial content (206).
- Supports seeking and progressive playback seamlessly within HTML5 video elements.

## Security Features

- **Encryption**: Forced RC4 encryption for BitTorrent peers.
- **HTTPS**: Exposed securely via TLS/HTTPS on port `443` relying on local network restrictions (Often proxied locally through an Nginx or Firewall rule), while passing down to internal C++ process at `0.0.0.0:8000`.
- **CORS**: Strict lockdown natively allowing only `http://localhost:1420` cross-origin requests.
- **Input Validation**: Strict boolean and 64-bit integer JSON casting.

## Core API Endpoints

### Content Discovery & State (Frontend Managed)

- `GET /api/v1/discover/*` - Native Content Discovery (Movies, Series, Anime)
- `POST /api/v1/user/history` - Sync Frontend History to DB
- `GET /api/v1/user/history` - Initialize Session Cache Let
- `POST /api/v1/user/viewlater` - Toggle favorite
- `GET /api/v1/user/viewlater` - Fetch favorites

### Torrent Management

- `POST /api/v1/torrents` - Add magnet link for downloading
- `DELETE /api/v1/torrents/{infoHash}` - Native backend torrent deletion handler
- `GET /api/v1/status` - Backup REST active torrent checks
- `wss://{server-ip}:443/api/v1/ws/status` - Live torrent socket

### Streaming

- `GET /api/v1/stream/{infoHash}` - Native HTML5 HTTP Range stream. (Must be handled via specific `<video src=...>` mounting).

## Build Instructions

### Server

```bash
cd server
mkdir build && cd build
conan install .. --build=missing -s compiler.cppstd=20
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./mediastream_server
```

### Client (Tauri Desktop)

```bash
cd clientTauri
npm install
npm run tauri dev
```

## Configuration

### Server Configuration

- **Oat++ Internal Binding**: `0.0.0.0:8000` (API)
- **External Exposure**: Exposed on LAN over HTTPS `443` (Requires reverse-proxy/firewall to pass 443 -> 8000 internally).
- **Download directory**: `./downloads`
- **Database**: `./mediastream.db`

### Client Configuration

- **Server Binding IP**: Managed via `.env` inside `clientTauri`.

## Future Enhancements

1. **Subtitle integration** (OpenSubtitles API in Frontend).
2. **TV Series Episode Tracking**.
3. **Transcoding** for incompatible audio/video formats.
4. **Android App** via Tauri mobile bindings.
