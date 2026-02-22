# MediaStream Server V2 - Application Workflow & Architecture

This document outlines the current state, architecture, and the workarounds implemented to successfully build and run the MediaStream server.

## Overview
MediaStream is a C++ backend utilizing **Oat++** for its high-performance asynchronous REST API, **libtorrent** to handle direct-to-memory media downloading, and **SQLite** for persisting history, torrent statuses, and media metadata.

The server operates as a unified proxy to discover movies/anime/series from public trackers (YTS, EZTV, Nyaa), enriches them with rich metadata (TMDB), downloads them via BitTorrent, and streams those actively-downloading files via an HTTP Range endpoint to front-end media players (like Qt Multimedia).

---

## Technical Application Flow

### 1. Content Discovery (`/api/v1/discover/movies`)
Instead of the frontend polling multiple APIs directly, the server acts as an aggregator:
1. `ContentDiscoveryManager` spins up requests to the `YTSClient`, `EZTVClient`, and `NyaaClient`.
2. It processes the tracker responses (combining seeds/leeches and parsing magnet links).
3. It passes the discovered items sequentially to `TMDBFetcher`, which queries `api.themoviedb.org` to enrich the results with high-resolution posters, backdrops, and proper descriptions.
4. An `oatpp` serialized JSON array is returned to the client.

### 2. Torrent Addition (`POST /api/v1/torrents`)
1. The frontend isolates a magnet link from the discovery array and posts it to the server.
2. `TorrentController` validates the payload and pushes it to the `TorrentEngine`.
3. The `TorrentEngine` translates the magnet into `libtorrent::add_torrent_params`, configuring the download path (`./downloads`), and begins fetching the metadata network.

### 3. Engine Status Synchronization (`GET /api/v1/status`)
1. An endpoint that provides a snapshot of the current active `libtorrent` sessions.
2. Returns total download rates, seeders, and current progress. The client uses the active `info_hash` later to request the specific video stream.

### 4. Direct HTTP Video Streaming (`GET /api/v1/stream/{infoHash}`)
1. The system locates the largest file inside the active torrent (typically the `.mp4` or `.mkv` video file).
2. The user's media player issues an HTTP request proposing a specific chunk of bytes using the `Range` header (e.g., `bytes=0-4096`).
3. The controller parses the start and end bytes. If the torrent has already downloaded that specific byte range physically to the `./downloads` disk, `std::ifstream` extracts that chunk.
4. To protect system memory and ensure rapid delivery without exhausting TCP windows, chunks are forcibly clamped to **4 Megabytes** maximum. 
5. Responds with HTTP `206 Partial Content`, allowing the Qt Media Player (or a web browser) to buffer and seek cleanly while it downloads.

---

## Critical Fixes and Workarounds Implemented

During compilation and runtime testing against **oatpp 1.3.0** and the Ubuntu C++ compiler stack, several breaking API differences and runtime edge cases were addressed:

### 1. Oat++ ORM Syntax Hallucinations
**The Problem:** The legacy codebase assumed `oatpp::orm::QueryResult` allowed direct row-by-row fetching (e.g., `row->getInt32(0)`). In Oat++ 1.3.0, this API was completely removed, halting the build.
**The Fix:** We injected deeply nested **Oat++ DTO macros** (Data Transfer Objects) directly inside `Database.cpp` (e.g., `class MediaItemDto : public oatpp::DTO`). `DbClient::executeQuery` was then refactored to cleanly map typed responses: `result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>()`.

### 2. YTS DNS & Dynamic URL Overrides
**The Problem:** The proxy domain `yts.mx` often fails to resolve via WSL or standard Ubuntu DNS. Additionally, hardcoding strings threw a severe initialization exception (`basic_string: construction from null is not valid`) when `std::getenv` failed to find the `.env` configuration.
**The Fix:** `YTSClient` was refactored with a safety operator to check for a valid `.env` payload: `const char* env_url = std::getenv("YTS_URL");`. If `nullptr`, it gracefully falls back to `https://yts.bz/api/v2`.

### 3. Oat++ `StreamController` Header Strictness
**The Problem:** The streaming endpoint used the Oat++ automatic macro `HEADER(String, range, "Range")`. While mathematically correct, this caused Oat++ to rigidly return `400 Bad Request` if a client (such as VLC or a naive curl call) pinged the URL *without* an explicit range header.
**The Fix:** We stripped the strict requirement out of the EndPoint specification. Instead, we manually query `REQUEST(std::shared_ptr<IncomingRequest>, request)` and gracefully check if `request->getHeader("Range")` is present within the runtime block.

### 4. C-String Null Pointer Traps
**The Problem:** A lingering check using `range->c_str().empty()` threw compilation errors because `c_str()` returns a raw `const char*` pointer which lacks object methods.
**The Fix:** Safely rewritten using native C bounds checking (`range->c_str()[0] != '\0'`).

### 5. `std_str()` Invalid Memory Access
**The Problem:** Attempts to extract `std::string` objects directly from Oat++ parameters using `std_str()` failed compilation as the accessor does not exist natively on the base object in this build.
**The Fix:** Ran an automated Python AST script across the workspace to universally substitute all string unpacking routes to utilize standard `c_str()` bindings.

---

## Running the Application
Ensure the `.env` file containing `TMDB_APIKEY_AT` is alongside your executable.
```bash
./build/mediastream_server
```
