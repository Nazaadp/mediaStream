# MediaStream Architecture

## Overview
MediaStream is a private Netflix-like application that discovers, downloads, and streams torrents locally via HTTPS. Built entirely in C++ with a Qt desktop client.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Qt Desktop Client                        │
│  (C++ / Qt5 Widgets / Qt5 Network)                          │
│  - Browse content catalog                                    │
│  - Search movies/series/anime                                │
│  - Stream video with progress tracking                       │
│  - Manage downloads                                          │
└────────────────────┬────────────────────────────────────────┘
                     │ HTTPS REST API
                     │
┌────────────────────▼────────────────────────────────────────┐
│                  MediaStream Server                          │
│  (C++ / oatpp / libtorrent)                                 │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              REST API Layer (oatpp)                   │  │
│  │  - Content discovery endpoints                        │  │
│  │  - Torrent management                                 │  │
│  │  - Video streaming (HTTP Range requests)              │  │
│  │  - Watch history tracking                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │           Content Discovery Services                  │  │
│  │  - YTS Client (Movies)                                │  │
│  │  - EZTV Client (TV Series)                            │  │
│  │  - Nyaa Client (Anime)                                │  │
│  │  - TMDB Metadata Fetcher                              │  │
│  └──────────────────────────────────────────────────────┘  │
│                           │                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            Core Services                              │  │
│  │  - TorrentEngine (libtorrent)                         │  │
│  │    * Sequential download for streaming                │  │
│  │    * Torrent management                               │  │
│  │  - Database (SQLite via oatpp-sqlite)                 │  │
│  │    * Media metadata                                   │  │
│  │    * Torrent info                                     │  │
│  │    * Watch history                                    │  │
│  │    * Episode tracking                                 │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Technology Stack

### Server
- **Language**: C++20
- **HTTP Framework**: oatpp 1.3.0
- **Torrent Engine**: libtorrent 2.0.10
- **Database**: SQLite (via oatpp-sqlite 1.3.0)
- **HTTP Client**: libcurl 8.4.0
- **JSON**: nlohmann_json 3.11.3
- **Logging**: spdlog 1.12.0

### Client
- **Language**: C++17
- **UI Framework**: Qt5 (Widgets, Network)
- **Video Player**: Qt Multimedia (planned)

## Database Schema

### media_items
Stores all discovered content (movies, series, anime)
- id, type, title, year, description
- poster_url, backdrop_url, rating, genres
- runtime_minutes, tmdb_id, imdb_id
- created_at, updated_at

### torrents
Stores torrent information linked to media
- id, media_id, info_hash, magnet_uri
- quality, size_bytes, seeders, leechers
- source, status, progress, file_path
- created_at, updated_at

### watch_history
Tracks user viewing progress
- id, media_id, position_seconds
- duration_seconds, progress_percent
- last_watched, completed

### episodes
Stores TV series/anime episode information
- id, media_id, season_number, episode_number
- title, description, still_url
- runtime_minutes, air_date

## Content Discovery Flow

1. **Discovery Manager** queries all sources (YTS, EZTV, Nyaa)
2. **API Clients** fetch popular/trending content
3. **TMDB Fetcher** enriches metadata (posters, descriptions, ratings)
4. **Database** stores discovered content
5. **Client** displays catalog with posters and info

## Download & Streaming Flow

1. **User selects** content from catalog
2. **Client requests** download via REST API
3. **Server** adds magnet to TorrentEngine with sequential download
4. **TorrentEngine** downloads pieces sequentially
5. **Client streams** video via HTTP Range requests
6. **Server** serves file chunks as they download
7. **Watch history** tracks progress automatically

## Streaming Protocol

**HTTP Range Requests** (RFC 7233)
- Client requests specific byte ranges
- Server responds with partial content (206)
- Supports seeking and progressive playback
- Works seamlessly with HTML5 video and Qt Multimedia
- Efficient for sequential torrent downloads

## Security Features

- **Encryption**: Forced RC4 encryption for torrent traffic
- **HTTPS**: SSL/TLS for client-server communication (planned)
- **Input Validation**: Magnet URI validation
- **Non-root**: Server refuses to run as root
- **File Permissions**: Restrictive umask (0077)

## API Endpoints (Planned)

### Content Discovery
- `GET /api/v1/discover/popular` - Get popular content
- `GET /api/v1/discover/movies` - Get movies
- `GET /api/v1/discover/series` - Get TV series
- `GET /api/v1/discover/anime` - Get anime
- `GET /api/v1/search?q={query}` - Search all content

### Torrent Management
- `POST /api/v1/torrents` - Add torrent
- `GET /api/v1/torrents` - List active torrents
- `GET /api/v1/torrents/{hash}` - Get torrent status
- `DELETE /api/v1/torrents/{hash}` - Remove torrent

### Streaming
- `GET /api/v1/stream/{media_id}` - Stream video (Range support)
- `POST /api/v1/watch-history` - Update watch progress
- `GET /api/v1/watch-history` - Get recently watched

### Media Library
- `GET /api/v1/library` - Get downloaded media
- `GET /api/v1/library/{id}` - Get media details
- `DELETE /api/v1/library/{id}` - Delete media

## Current Implementation Status

### ✅ Completed
1. Database schema and implementation
2. Content discovery services (YTS, EZTV, Nyaa)
3. TorrentEngine with sequential download
4. Basic REST API structure
5. Qt client foundation

### 🚧 In Progress
- TMDB metadata fetcher
- HTTP streaming endpoint with Range support
- Content manager orchestration

### 📋 Pending
- Enhanced REST API endpoints
- Qt client UI improvements
- Watch history tracking
- Search and filtering
- SSL/TLS support
- Video player integration

## Build Instructions

### Server
```bash
cd server
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
./mediastream_server
```

### Client
```bash
cd client
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
./client
```

## Configuration

### Server
- Port: 8000 (configurable)
- Download directory: `./downloads`
- Database: `./mediastream.db`
- Max connections: 200
- Active downloads: 3

### Client
- Server IP: Configurable in UI
- Server Port: 8000
- Theme: Dark (Netflix-style)

## Future Enhancements

1. **Multi-user support** with authentication
2. **Subtitle integration** (OpenSubtitles API)
3. **Transcoding** for incompatible formats
4. **Mobile app** (Qt for Android/iOS)
5. **Web interface** as alternative to desktop client
6. **Plex/Jellyfin integration** for existing libraries
7. **Automatic quality selection** based on bandwidth
8. **Download scheduling** and bandwidth limits
9. **Content recommendations** based on watch history
10. **Chromecast/DLNA support** for TV streaming

## License

Private use only. Not for distribution.