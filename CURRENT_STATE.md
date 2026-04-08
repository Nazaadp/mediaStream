
# ClientTauri - Current State Documentation

## 1. Current State

### Development Stage
- **Phase**: MVP Development / Active Development
- **Maturity**: Early-stage functional prototype
- **Stability**: Core features implemented, UI/UX refinement ongoing
- **Architecture**: Hybrid desktop application (Tauri 2.0 + SvelteKit 2.x)

### Component Status
| Component | Status | Notes |
|-----------|--------|-------|
| Main Browse UI | ✅ Stable | Netflix-inspired gallery layout with horizontal scrolling |
| Video Player | ✅ Stable | Custom HTML5 player with streaming support |
| Search System | ✅ Functional | Real-time search with keyboard navigation |
| Filter System | ✅ Functional | Genre and language filtering |
| WebSocket Integration | ✅ Stable | Real-time torrent status updates |
| Watch History | ✅ Functional | Session-based tracking with backend sync |
| View Later | ✅ Functional | Bookmark system with instant cache updates |
| Tauri Backend | ⚠️ Minimal | Basic shell with unused `greet` command |

### Known Limitations
- Season/Episode selector is hardcoded (UI placeholder only)
- No actual settings page implementation
- Filter system only works in search mode
- Navigation links (Movies, Series, Anime) are non-functional
- No subtitle support implemented
- Tauri Rust backend is underutilized (no custom commands beyond template)

---

## 2. Current Capabilities

### Content Discovery
- **Multi-source aggregation**: Movies (YTS), Series (EZTV), Anime (Nyaa)
- **Pagination**: Infinite scroll with lazy loading for all content types
- **Caching**: LocalStorage for discovery data, SessionStorage for user data
- **Metadata enrichment**: TMDB integration for posters, backdrops, descriptions

### Search & Filtering
- **Real-time search**: Query endpoint `/api/v1/discover/search`
- **Content categorization**: Automatic split into Movies/Series/Anime based on type and language
- **Genre filtering**: 18 predefined genres (Action, Drama, Sci-Fi, etc.)
- **Language filtering**: 8 languages (English, Japanese, Spanish, French, German, Korean, Chinese)
- **Keyboard navigation**: Arrow keys for grid navigation, Enter to select, Escape to exit

### Media Playback
- **Instant streaming**: HTTP Range request-based progressive download
- **Custom player controls**: Play/pause, timeline scrubbing, volume control
- **Buffering intelligence**: 
  - Waits for 5% download before initializing stream
  - Blocks on "Fetching Metadata" and "Checking" states
  - Automatic retry logic for 503 errors (20 retries × 2s = 40s max)
- **Codec diagnostics**: 
  - Detects HEVC/H.265 unsupported codec issues
  - Distinguishes between codec errors and moov atom loading delays
  - User-friendly hints for missing HEVC Video Extensions
- **Progress tracking**: Real-time buffer percentage badge
- **Watch history**: Automatic save on navigation/close (>95% = completed)

### Torrent Management
- **Download initiation**: POST to `/api/v1/torrents` with magnet URI
- **Status monitoring**: 
  - WebSocket connection to `/api/v1/ws/status`
  - Fallback REST polling every 2s
  - Real-time progress indicators (circular SVG progress bars)
- **Quality selection**: Multiple torrent options per media item (quality, type, size, seeders/leechers)

### User Features
- **Watch History**: Session-persistent list with proactive refresh on focus
- **View Later**: Bookmark system with instant optimistic updates
- **Dirty flag system**: `mediaStream_history_dirty` localStorage flag triggers refresh on return from player

### UI/UX
- **Netflix-inspired design**: Dark theme with red accent color (#E50914)
- **Responsive layout**: Mobile-first grid system (140px → 180px → 220px cards)
- **Smooth animations**: Hover effects, modal transitions, scroll behaviors
- **Custom scrollbars**: Sleek 8px width with transparent track
- **Accessibility**: ARIA labels, keyboard navigation, focus indicators

---

## 3. Current Characteristics

### Technology Stack

#### Frontend Framework
- **SvelteKit**: 2.53.2 (SPA mode with adapter-static)
- **Svelte**: 5.0.0 (Runes-based reactivity)
- **Vite**: 6.0.3 (Build tool and dev server)

#### Desktop Framework
- **Tauri**: 2.10.0 (Rust-based native wrapper)
- **Tauri Plugins**:
  - `tauri-plugin-http@2.5.7`: HTTP client for secure requests
  - `tauri-plugin-opener@2`: System file/URL opener

#### Styling
- **Vanilla CSS**: Custom properties (CSS variables) for theming
- **Google Fonts**: Inter font family (400, 500, 600, 700 weights)
- **Design System**: Netflix-inspired color palette and spacing scale

#### Build Tools
- **TypeScript**: 5.6.2 (type checking only, no TS source files)
- **svelte-check**: 4.0.0 (Svelte component validation)

### Architecture Patterns

#### Application Architecture
```
┌─────────────────────────────────────────┐
│         Tauri Native Shell              │
│  (Rust + WebView2/WebKit/WKWebView)     │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │      SvelteKit SPA Frontend       │ │
│  │  ┌─────────────────────────────┐  │ │
│  │  │   Routes (File-based)       │  │ │
│  │  │   - / (Browse)              │  │ │
│  │  │   - /watch/[infoHash]       │  │ │
│  │  └─────────────────────────────┘  │ │
│  │  ┌─────────────────────────────┐  │ │
│  │  │   Components (Reusable)     │  │ │
│  │  │   - PosterCard              │  │ │
│  │  │   - MediaCard (Modal)       │  │ │
│  │  │   - FilterBar               │  │ │
│  │  └─────────────────────────────┘  │ │
│  └───────────────────────────────────┘ │
└─────────────────────────────────────────┘
         ↓ HTTPS (Self-signed TLS)
┌─────────────────────────────────────────┐
│      Backend REST API + WebSocket       │
│   (oatpp C++ server on 127.0.0.1:8000)  │
└─────────────────────────────────────────┘
```

#### Component Hierarchy
```
+layout.svelte (Global CSS import)
├── +page.svelte (Main Browse Page)
│   ├── PosterCard.svelte (×N for each media item)
│   ├── MediaCard.svelte (Modal overlay)
│   └── FilterBar.svelte (Search filters)
└── watch/[infoHash]/+page.svelte (Video Player)
```

#### State Management
- **No global store**: Component-local state with Svelte's reactive declarations
- **Cache layers**:
  - `localStorage.mediaStreamCache`: Discovery data (movies, series, anime)
  - `sessionStorage.historyCache`: Watch history
  - `sessionStorage.viewLaterCache`: Bookmarked items
  - `localStorage.mediaStream_history_dirty`: Refresh trigger flag

### Configuration Management

#### Environment Variables
```javascript
// .env.example
VITE_API_URL=https://192.168.1.37:443  // Backend REST API
VITE_WS_URL=wss://192.168.1.37:443     // WebSocket endpoint
```

#### Tauri Configuration
- **Window**: 800×600 default size
- **Browser Args**: 
  - `--ignore-certificate-errors`: Accept self-signed TLS certificates
  - `--enable-features=PlatformHEVCDecoderSupport`: Enable HEVC/H.265 hardware decoding
- **CSP**: Disabled (`null`) for development flexibility
- **Bundle**: Multi-platform targets (Windows, macOS, Linux)

#### SvelteKit Configuration
- **SSR**: Disabled (`export const ssr = false` in `+layout.js`)
- **Adapter**: `adapter-static` with `fallback: "index.html"` for SPA mode
- **Dev Server**: Port 1420 (strict), HMR on port 1421

### Dependencies

#### Production Dependencies
```json
{
  "@tauri-apps/api": "^2.10.1",
  "@tauri-apps/plugin-http": "^2.5.7",
  "@tauri-apps/plugin-opener": "^2"
}
```

#### Development Dependencies
```json
{
  "@sveltejs/adapter-static": "^3.0.6",
  "@sveltejs/kit": "^2.9.0",
  "@sveltejs/vite-plugin-svelte": "^5.0.0",
  "@tauri-apps/cli": "^2",
  "svelte": "^5.0.0",
  "svelte-check": "^4.0.0",
  "typescript": "~5.6.2",
  "vite": "^6.0.3"
}
```

### Performance Characteristics
- **Initial load**: Fast (SPA with cached discovery data)
- **Infinite scroll**: Efficient (Intersection Observer API)
- **WebSocket**: Persistent connection with auto-reconnect
- **Video streaming**: Progressive (HTTP Range requests, 5% threshold)
- **Cache strategy**: Aggressive (localStorage + sessionStorage)

---

## 4. Current Design

### Data Flow Patterns

#### Discovery Flow
```
User opens app
    ↓
onMount() in +page.svelte
    ↓
Check localStorage.mediaStream_history_dirty
    ↓
├─ If dirty: refreshData()
│   ├─ Fetch /api/v1/user/history
│   ├─ Fetch /api/v1/user/viewlater
│   ├─ Fetch /api/v1/discover/movies?page=1
│   ├─ Fetch /api/v1/discover/series?page=1
│   ├─ Fetch /api/v1/discover/anime?page=1
│   └─ Save to localStorage + sessionStorage
│
└─ If clean: Load from localStorage.mediaStreamCache
    └─ If cache miss: refreshData()
```

#### Search Flow
```
User types in search input
    ↓
handleSearchKeydown() on Enter
    ↓
performSearch()
    ↓
Fetch /api/v1/discover/search?query=...
    ↓
Categorize results by type and language
    ├─ Movies: type === 'movie'
    ├─ Series: type === 'tv' && language !== 'ja|zh|ko'
    └─ Anime: type === 'tv' && language === 'ja|zh|ko'
    ↓
Render in search grid with keyboard navigation
```

#### Streaming Flow
```
User clicks "▶ Stream" on torrent
    ↓
handlePlay() in +page.svelte
    ↓
POST /api/v1/torrents { magnet_link }
    ↓
Navigate to /watch/[infoHash]
    ↓
Video Player Component
    ├─ fetchInitialStatus() (REST)
    ├─ connectWebSocket() (WS)
    └─ startPolling() (Fallback)
    ↓
Wait for torrentProgress > 5% && state !== "Fetching Metadata|Checking"
    ↓
Set videoSrc = /api/v1/stream/[infoHash]
    ↓
Browser initiates HTTP Range request
    ↓
Backend streams torrent pieces sequentially
    ↓
Video plays with real-time buffer updates via WebSocket
```

#### Watch History Flow
```
User watches video
    ↓
onDestroy() or goBack() in watch/[infoHash]/+page.svelte
    ↓
saveWatchHistory()
    ├─ Read position/duration from videoElement
    ├─ Find media object in caches (localStorage/sessionStorage)
    └─ POST /api/v1/user/history { media, position, duration, progress, completed }
    ↓
Set localStorage.mediaStream_history_dirty = "true"
    ↓
User returns to browse page
    ↓
onMount() detects dirty flag → refreshData()
```

### Communication Protocols

#### REST API Endpoints
| Method | Endpoint | Purpose | Request Body | Response |
|--------|----------|---------|--------------|----------|
| GET | `/api/v1/discover/movies?page=N` | Fetch paginated movies | - | `Array<Media>` |
| GET | `/api/v1/discover/series?page=N` | Fetch paginated series | - | `Array<Media>` |
| GET | `/api/v1/discover/anime?page=N` | Fetch paginated anime | - | `Array<Media>` |
| GET | `/api/v1/discover/search?query=X` | Search all content | - | `Array<Media>` |
| GET | `/api/v1/user/history` | Get watch history | - | `Array<Media>` |
| GET | `/api/v1/user/viewlater` | Get bookmarks | - | `Array<Media>` |
| POST | `/api/v1/user/history` | Save watch progress | `{ media, position_seconds, duration_seconds, progress_percent, completed }` | `200 OK` |
| POST | `/api/v1/user/viewlater` | Toggle bookmark | `{ media, saved }` | `200 OK` |
| POST | `/api/v1/torrents` | Add torrent | `{ magnet_link }` | `200 OK` |
| GET | `/api/v1/status` | Get all torrent statuses | - | `Array<TorrentStatus>` |
| GET | `/api/v1/stream/[infoHash]` | Stream video (Range requests) | - | `206 Partial Content` |

#### WebSocket Protocol
- **Endpoint**: `wss://[host]/api/v1/ws/status`
- **Message Format**: JSON array of torrent statuses
```json
[
  {
    "info_hash": "abc123...",
    "progress": 0.42,
    "state": "Downloading"
  }
]
```
- **Reconnect Logic**: Auto-reconnect on close with 2s delay
- **Lifecycle**: Persistent connection throughout app session

#### Tauri IPC (Inter-Process Communication)
- **Current Usage**: Minimal (only template `greet` command)
- **Available Plugins**:
  - `tauri-plugin-http`: Not actively used (frontend uses native `fetch`)
  - `tauri-plugin-opener`: Not actively used
- **Potential**: Underutilized for native features (file system, notifications, etc.)

### Event Handling Systems

#### Browser Events
- **Keyboard**: 
  - Global `keydown` listener for search navigation
  - Arrow keys (Up/Down/Left/Right) for grid navigation
  - Enter to select, Escape to exit
- **Mouse**:
  - `click` for card selection and player controls
  - `mousemove` for player control visibility
  - `mouseleave` to hide controls
- **Video**:
  - `waiting`: Set buffering state
  - `playing`: Clear buffering state
  - `loadedmetadata`: Codec diagnostic check
  - `error`: Retry logic for 503 errors
  - `pause`/`play`: Update UI state

#### Custom Events (Svelte Dispatchers)
```javascript
// PosterCard.svelte
dispatch('select', item);

// MediaCard.svelte
dispatch('close');
dispatch('play', { media, torrent });
dispatch('download', { media, torrent });

// FilterBar.svelte
dispatch('change', { selectedGenres, selectedLanguage });
```

### Design Patterns

#### Component Patterns
- **Presentational Components**: `PosterCard`, `FilterBar` (pure UI, no business logic)
- **Container Components**: `+page.svelte`, `watch/[infoHash]/+page.svelte` (data fetching + state)
- **Modal Pattern**: `MediaCard` (overlay with backdrop click-to-close)
- **Infinite Scroll**: Custom Svelte action with Intersection Observer

#### State Patterns
- **Optimistic Updates**: View Later toggle updates cache immediately before API call
- **Dirty Flag**: `mediaStream_history_dirty` triggers refresh on navigation
- **Fallback Polling**: REST polling as safety net when WebSocket is slow
- **Cache-First**: Load from cache, then refresh in background

#### Error Handling Patterns
- **Retry Logic**: Video player retries 503 errors up to 20 times
- **Graceful Degradation**: Show empty states instead of errors
- **User Feedback**: Codec hints, buffering spinners, progress badges

---

## 5. Current Requests

### API Request Mapping

#### Discovery Requests
```javascript
// Movies (Infinite Scroll)
GET ${VITE_API_URL}/api/v1/discover/movies?page=${moviePage}
  &genre=${selectedGenres.join(",")}
  &language=${selectedLanguage}

// Series (Infinite Scroll)
GET ${VITE_API_URL}/api/v1/discover/series?page=${seriesPage}
  &genre=${selectedGenres.join(",")}
  &language=${selectedLanguage}

// Anime (Infinite Scroll)
GET ${VITE_API_URL}/api/v1/discover/anime?page=${animePage}
  &genre=${selectedGenres.join(",")}
  &language=${selectedLanguage}

// Search
GET ${VITE_API_URL}/api/v1/discover/search?query=${encodeURIComponent(searchQuery)}
```

#### User Data Requests
```javascript
// Watch History
GET ${VITE_API_URL}/api/v1/user/history

POST ${VITE_API_URL}/api/v1/user/history
// Body: { media, position_seconds, duration_seconds, progress_percent, completed }

// View Later
GET ${VITE_API_URL}/api/v1/user/viewlater

POST ${VITE_API_URL}/api/v1/user/viewlater
// Body: { media, saved }
```

#### Torrent Requests
```javascript
// Add Torrent
POST ${VITE_API_URL}/api/v1/torrents
// Body: { magnet_link }

// Get Status (REST Fallback)
GET ${VITE_API_URL}/api/v1/status

// Stream Video (HTTP Range Requests)
GET ${VITE_API_URL}/api/v1/stream/${infoHash}
// Headers: Range: bytes=0-1023 (browser-managed)
```

### Request Characteristics

#### Error Handling
```javascript
try {
  const res = await fetch(url);
  if (res.ok) {
    const data = await res.json();
  } else {
    alert(`Error: ${res.statusText}`);
  }
} catch (e) {
  console.error("Network error:", e);
  alert("Failed to connect to backend");
}
```

#### Retry Logic
- **Video Streaming**: 20 retries with 2s delay for 503 errors
- **WebSocket**: Infinite reconnect with 2s delay
- **REST Polling**: Every 2s until `isReadyToPlay` is true

#### Caching Strategy
```javascript
// Write-through cache (Discovery)
const cacheData = { movies, series, anime, timestamp: Date.now() };
localStorage.setItem("mediaStreamCache", JSON.stringify(cacheData));

// Optimistic update (View Later)
sessionStorage.setItem("viewLaterCache", JSON.stringify(updatedList));
await fetch(url, { method: "POST", body: JSON.stringify(payload) });
```

---

## 6. Current Workflows

### Initialization Sequence
```
1. Tauri launches native window
2. WebView loads SvelteKit SPA from localhost:1420 (dev) or file:// (prod)
3. SvelteKit router initializes, loads +layout.svelte (global CSS)
4. +page.svelte mounts
5. onMount() executes:
   a. Check sessionStorage for historyCache/viewLaterCache
   b. Check localStorage.mediaStream_history_dirty
   c. If dirty OR no cache: refreshData()
   d. If clean: Load from localStorage.mediaStreamCache
6. connectWebSocket() establishes persistent connection
7. UI renders with cached/fetched data
```

### Critical User Journeys

#### Journey 1: Browse → Stream → Watch
```
1. User opens app
2. Browse page loads with cached/fresh discovery data
3. User scrolls through "Popular Movies" row
4. User clicks on a movie poster
5. MediaCard modal opens with movie details
6. User selects a torrent quality (e.g., "1080p BluRay")
7. User clicks "▶ Stream"
8. POST /api/v1/torrents adds magnet to backend
9. Navigate to /watch/[infoHash]
10. Video player shows "Establishing connection..." spinner
11. WebSocket receives progress updates (0% → 5%)
12. At 5%, videoSrc is set, browser requests stream
13. Backend serves video via HTTP Range requests
14. Video plays with real-time buffer badge
15. User watches, scrubs timeline, adjusts volume
16. User clicks "Back to Browse"
17. saveWatchHistory() POSTs progress to backend
18. Navigate back to browse page
19. Watch History row shows the movie with progress indicator
```

#### Journey 2: Search → Filter → Play
```
1. User clicks search icon in navbar
2. Search input appears and focuses
3. User types "inception"
4. User presses Enter
5. performSearch() fetches /api/v1/discover/search?query=inception
6. Results categorized into Movies/Series/Anime
7. Search grid renders with keyboard navigation
8. User presses Down arrow to enter grid
9. User navigates with arrow keys (focused card highlighted)
10. User presses Enter on selected card
11. MediaCard modal opens
12. User clicks "▶ Stream"
```

---

## 7. Current Possible Issues or Bugs

### Code Smells

#### 1. Unused Tauri Backend
**Location**: `src-tauri/src/lib.rs:3-5`
**Issue**: Template `greet` command is never called from frontend
**Impact**: Low (functional, but missed opportunity)
**Recommendation**: Remove unused command or implement native features

#### 2. Hardcoded Season/Episode Selector
**Location**: `src/lib/MediaCard.svelte:217-229`
**Issue**: UI placeholder with no backend integration
**Impact**: Medium (misleading UI)
**Recommendation**: Remove UI or implement dynamic fetching

#### 3. Non-functional Navigation Links
**Location**: `src/routes/+page.svelte:406-410`
**Issue**: Only "Home" has click handler
**Impact**: Low (UI inconsistency)
**Recommendation**: Implement filtering or remove links

#### 4. Magic Numbers in Code
**Location**: `src/routes/watch/[infoHash]/+page.svelte:71`
**Issue**: Hardcoded 5% threshold without named constant
**Impact**: Low (maintainability)
**Recommendation**: Extract to `const MIN_BUFFER_THRESHOLD = 0.05;`

### Security Vulnerabilities

#### 1. Disabled CSP
**Location**: `src-tauri/tauri.conf.json:22`
**Issue**: No CSP protection against XSS attacks
**Impact**: High (security risk)
**Recommendation**: Enable CSP with appropriate directives

#### 2. Certificate Validation Disabled
**Location**: `src-tauri/tauri.conf.json:18`
**Issue**: Accepts any TLS certificate
**Impact**: High (MITM vulnerability)
**Recommendation**: Use proper certificate validation

#### 3. No Input Sanitization
**Location**: `src/routes/+page.svelte:307`
**Issue**: No validation of search query length or content
**Impact**: Low (backend should validate)
**Recommendation**: Add client-side validation

### Performance Bottlenecks

#### 1. Synchronous Cache Operations
**Location**: `src/routes/+page.svelte:144-158`
**Issue**: Synchronous storage operations block main thread
**Impact**: Low (small datasets)
**Recommendation**: Debounce writes

#### 2. No Virtualization for Large Lists
**Location**: `src/routes/+page.svelte:583-585`
**Issue**: Renders all items in DOM
**Impact**: Medium (DOM bloat)
**Recommendation**: Implement virtual scrolling

### Technical Debt

#### 1. No TypeScript in Source Files
**Issue**: TypeScript installed but not used
**Impact**: Medium (type safety)
**Recommendation**: Migrate to `.ts` and `<script lang="ts">`

#### 2. No Component Tests
**Issue**: No unit tests, integration tests, or E2E tests
**Impact**: High (regression risk)
**Recommendation**: Add Vitest for unit tests, Playwright for E2E

#### 3. No Linting Configuration
**Issue**: No ESLint, Prettier, or Stylelint
**Impact**: Medium (code consistency)
**Recommendation**: Add ESLint + Prettier with Svelte plugins

### Accessibility Issues

#### 1. Missing ARIA Labels
**Location**: `src/routes/+page.svelte:527-543`
**Issue**: Scroll buttons have no `aria-label`
**Impact**: Medium (screen reader users)
**Recommendation**: Add `aria-label="Scroll left"` and `aria-label="Scroll right"`

#### 2. Keyboard Trap in Modal
**Location**: `src/lib/MediaCard.svelte:155-385`
**Issue**: No focus trap in modal
**Impact**: Medium (keyboard navigation)
**Recommendation**: Implement focus trap

---

## Summary

The clientTauri component is a functional MVP with a solid foundation but several areas for improvement:

**Strengths**:
- Clean Netflix-inspired UI with smooth animations
- Robust video streaming with intelligent retry logic
- Real-time torrent status updates via WebSocket
- Efficient caching strategy for offline-first experience
- Comprehensive codec diagnostics for user feedback

**Weaknesses**:
- Underutilized Tauri backend (no native features)
- No TypeScript usage despite installation
- Missing test coverage
- Security concerns (disabled CSP, certificate validation)
- Technical debt (hardcoded values, code duplication)
- Accessibility gaps (ARIA labels, focus management)

**Priority Fixes**:
1. Enable CSP and proper certificate validation (Security)
2. Implement proper error handling with user feedback (UX)
3. Add TypeScript for type safety (Code Quality)
4. Remove or implement season/episode selector (UI Consistency)
5. Add unit and E2E tests (Reliability)