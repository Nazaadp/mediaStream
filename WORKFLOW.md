# MediaStream Server V2 - Application Workflow & Architecture

This document outlines the current state, architecture, and the workflows required to successfully build and run the MediaStream ecosystem (SvelteKit/Tauri Client + C++ Oat++ Server).

## Overview

MediaStream utilizes a unified C++ backend handling **Oat++** high-performance REST APIs, **libtorrent** for direct-to-memory media downloading, and **SQLite** for persisting history, torrent statuses, and media metadata.

The frontend is a strictly designed **SvelteKit + Tauri v2** Desktop Application that takes full responsibility for discovering content and managing API rate limits locally before deferring heavy download/streaming lifting to the C++ server.

---

## Technical Application Flow

### 1. Content Discovery (Backend Managed)

Instead of the frontend executing APIs directly and dealing with CORS or varied platform issues:

1. The **Tauri frontend** spins up requests to the internal Oat++ Server endpoints (`/api/v1/discover/movies`).
2. The **C++ Backend (`ContentDiscoveryManager`)** processes the fetch natively via embedded classes `TMDBFetcher`, `YTSClient`, `NyaaClient`, and `EZTVClient`.
3. The server acts as a unified aggregation layer, amalgamating the TMDB metadata alongside YTS/Nyaa Torrents into a single optimized DTO `vector`.
4. The client uses this data, and when a user saves a movie or watches history, the frontend POSTs the *entire* parsed JSON metadata payload back to the C++ backend (`/api/v1/user/history`). The backend safely upserts it into the SQLite `media_items` table.

### 2. Torrent Addition (`POST /api/v1/torrents`)

1. The frontend isolates a magnet link from its own discovery mechanisms and posts it to the server.
2. `TorrentController` (C++) validates the payload and pushes it to the `TorrentEngine`.
3. The `TorrentEngine` translates the magnet into `libtorrent::add_torrent_params`, configuring the download path (`./downloads`), and requests **sequential downloading** to prioritize start bytes.

### 3. Frontend Optimistic Caching & Syncing

1. **`localStorage`**: Caches Discovery data on the Home route (Top Movies, Trending Anime) to prevent re-fetching from TMDB on app startup.
2. **`sessionStorage`**: Continually caches dynamic user-specific states (Watch History arrays, View Later favorites arrays) initialized by a single `GET /api/v1/user/history` call on mount.
3. **Visual Reactivity**: When toggling favorites, the Svelte component updates the local array instantly to animate the icon, and dispatches the `POST` request silently in the background.

### 4. Direct HTTP Video Streaming (`GET /api/v1/stream/{infoHash}`)

1. On UI click, SvelteKit programmatically routes to `<video src="https://{server-ip}:443/api/v1/stream/{infoHash}">` (or `http://localhost:8000` via development proxy).
2. The user's HTML5 media player issues an HTTP request proposing a specific chunk of bytes using the `Range` header (e.g., `bytes=0-4096`).
3. The controller parses the start and end bytes. If the torrent has already downloaded that specific byte range physically to the `./downloads` disk, `std::ifstream` extracts that chunk.
4. To protect system memory and ensure rapid delivery without exhausting TCP windows, chunks are forcibly clamped to **4 Megabytes** maximum.
5. Responds with HTTP `206 Partial Content`, allowing the video player to buffer properly.
6. The `wss://{server-ip}:443/api/v1/ws/status` socket seamlessly updates the torrent progress.

---

## Critical Fixes and Workarounds Implemented

During compilation and runtime testing against **oatpp 1.3.0** and the Ubuntu C++ compiler stack, several breaking API differences and runtime edge cases were addressed:

### 1. Oat++ ORM Syntax Hallucinations

**The Problem:** The legacy codebase assumed `oatpp::orm::QueryResult` allowed direct row-by-row fetching (e.g., `row->getInt32(0)`). In Oat++ 1.3.0, this API was completely removed, halting the build.
**The Fix:** We injected deeply nested **Oat++ DTO macros** (Data Transfer Objects) directly inside `Database.cpp` (e.g., `class MediaItemDto : public oatpp::DTO`). `DbClient::executeQuery` was then refactored to cleanly map typed responses: `result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>()`.

### 2. Svelte/JS Payload Conversions (32-bit vs 64-bit)

**The Problem:** Parsing `size_bytes` from frontend JSON payloads for 4K/1080p movies defaulted to a 32-bit signed integer in C++, overflowing into negative numbers (>2.1GB).
**The Fix:** Enforced 64-bit parsing (`long long`) in `nlohmann::json` lookups on the backend, and added `Math.max()` defensive guards on the Svelte frontend.

### 3. Oat++ `StreamController` Header Strictness

**The Problem:** The streaming endpoint used the Oat++ automatic macro `HEADER(String, range, "Range")`. While mathematically correct, this caused Oat++ to rigidly return `400 Bad Request` if a client (such as VLC or a naive curl call) pinged the URL *without* an explicit range header.
**The Fix:** We stripped the strict requirement out of the EndPoint specification. Instead, we manually query `REQUEST(std::shared_ptr<IncomingRequest>, request)` and gracefully check if `request->getHeader("Range")` is present within the runtime block.

### 4. SQLite Multithreading ID Clashing

**The Problem:** Executing an `INSERT OR IGNORE` followed by `SELECT last_insert_rowid()` caused race conditions across Oat++ threads.
**The Fix:** Never use `last_insert_rowid()`. Always follow inserts with explicit robust `SELECT` queries locating the exact TMDB ID.

### 5. `std_str()` Invalid Memory Access

**The Problem:** Attempts to extract `std::string` objects directly from Oat++ parameters using `std_str()` failed compilation as the accessor does not exist natively on the base object in this build.
**The Fix:** Ran an automated Python AST script across the workspace to universally substitute all string unpacking routes to utilize standard `c_str()` bindings.

---

## Running the Application

Ensure the `.env` file containing configuration keys is correctly placed.

```bash
# Server
./build/mediastream_server
```

```bash
# Frontend
npm run tauri dev
```
