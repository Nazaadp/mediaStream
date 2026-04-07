# MediaStream — Streaming Pipeline Context (April 2026)

## Project Overview
Private Netflix-like application. C++20 backend (Oat++ 1.3.0, libtorrent 2.0.10, SQLite)
served via systemd on Ubuntu 24.04 KVM VM, reverse-proxied through Nginx TLS.
Frontend: Tauri 2.0 + SvelteKit + Vanilla JS.
Stream URL pattern: `https://192.168.1.37/api/v1/stream/<infoHash>`

---

## Streaming Issues Fixed This Session

### Issue 1 — WebSocket 400 error through Nginx
**Problem:** WS connection to `/api/v1/ws/status` was failing with HTTP 400 because Nginx
wasn't forwarding the `Upgrade` and `Connection` headers.
**Fix:** Added to Nginx config (`/etc/nginx/sites-enabled/mediastream`):
```nginx
location /api/v1/ws/ {
    proxy_pass http://127.0.0.1:8000;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}

location /api/v1/stream/ {
    proxy_pass http://127.0.0.1:8000;
    proxy_buffering off;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}
```

---

### Issue 2 — Video player restarting from 0:00 mid-movie
**Problem:** `$: videoSrc = isReadyToPlay ? url : null` was a Svelte reactive declaration.
Every WS reconnect (every 2s on close) re-assigned `isReadyToPlay = true`, which
re-triggered the reactive, re-assigning `videoSrc` to the same string — which
tells the browser to reload the video from byte 0, discarding playback position.
**Fix:** (`clientTauri/src/routes/watch/[infoHash]/+page.svelte`)
```js
// WRONG — reactive, re-evaluates on every isReadyToPlay write:
$: videoSrc = isReadyToPlay ? url : null;

// CORRECT — stable let, assigned exactly once:
let videoSrc = null;
// ...set only inside the WS/polling handler when isReadyToPlay transitions false→true
videoSrc = `${import.meta.env.VITE_API_URL}/api/v1/stream/${infoHash}`;
```

---

### Issue 3 — "Fetching Metadata" / "Checking" state guard
**Problem:** The frontend was initiating playback while the torrent was in
`checking_files` state (libtorrent rehashing pieces on resume). During checking,
`h.have_piece(0)` returns false even if data is on disk → `waitForPiece` → 503.
**Fix:** Block on both states:
```js
const blockedState = myTorrent.state === "Fetching Metadata"
                  || myTorrent.state === "Checking";
if (torrentProgress > 0.05 && !isReadyToPlay && !blockedState) { ... }
```

---

### Issue 4 — Backend serving zeros (pre-allocated file)
**Problem:** When pieces weren't ready, `StreamController` served zeros from
libtorrent's pre-allocated file to the browser → corrupted the video decoder.
**Fix:** (`server/include/mediastream/api/StreamController.hpp`)
Changed `waitForPiece` from `void` to `bool`. StreamController now:
```cpp
bool piece_ready = m_engine->waitForPiece(infoHash, start);
if (!piece_ready) {
    auto response = createResponse(Status::CODE_503, "Piece not yet available");
    response->putHeader("Retry-After", "2");
    return response;
}
```

---

### Issue 5 — Browser `<video>` doesn't retry on 503
**Problem:** The HTML5 `<video>` element treats 503 as `MEDIA_ERR_SRC_NOT_SUPPORTED`
(code 4) — a permanent fatal failure. It does NOT honor `Retry-After`.
**Fix:** (`+page.svelte`) — smart retry in `handleVideoError`:
```js
const MAX_STREAM_RETRIES = 20; // 20 × 2s = 40s window
let streamRetryCount = 0;

function handleVideoError(e) {
    const code = videoElement?.error?.code;
    const isTransient = code === 4 && streamRetryCount < MAX_STREAM_RETRIES;
    if (isTransient) {
        streamRetryCount++;
        setTimeout(() => {
            videoElement.src = videoSrc; // Re-assign same URL to force retry
            videoElement.load();
            videoElement.play().catch(() => {});
        }, 2000);
    } else {
        // code 3 = MEDIA_ERR_DECODE → codec failure (HEVC)
        noVideoHint = code === 3 ? 'codec' : 'moov';
    }
}
```

---

### Issue 6 — Moov atom at end of file (sound but no video)
**Root Cause:** MP4 files store container metadata (moov atom) at the END of the file.
The browser, after loading from piece 0, internally seeks to the end to read
the moov atom. With sequential download at 5%, piece 999 (end of a ~2GB movie)
is not yet downloaded. The browser silently plays audio (AAC from piece 0) but
cannot initialize the video decoder without the moov atom.

**Server log signature:**
```
Moov atom seek detected: boosting priority for pieces 999-999
Timeout waiting for piece 999 to download (moov_seek=true).
```

**Fix A — On-demand boost** (`server/src/core/TorrentEngine.cpp`, `waitForPiece`):
When the requested piece is in the last 15% of the torrent, boost ALL end pieces
and wait 4000ms instead of 500ms:
```cpp
const bool is_moov_seek = piece_idx_int > (total_pieces * 85 / 100);
if (is_moov_seek) {
    for (int i = piece_idx_int; i < total_pieces; ++i) {
        h.piece_priority(lt::piece_index_t(i), lt::top_priority);
        h.set_piece_deadline(lt::piece_index_t(i), (i - piece_idx_int) * 200);
    }
}
const int max_wait_ms = is_moov_seek ? 4000 : 500;
```

**Fix B — Proactive boost** (NEW — just implemented, needs build+deploy):
Every second in the WS broadcaster loop, `proactivelyBoostEndPieces()` is called.
As soon as a torrent enters `Downloading` state with metadata available, the last
5% of pieces are set to `top_priority`. This fires ~1 second after the torrent
resolves metadata — well before the user clicks Stream:
```cpp
// TorrentEngine.cpp
void TorrentEngine::proactivelyBoostEndPieces() {
    for (const auto& h : m_session.get_torrents()) {
        if (!h.torrent_file()) continue;
        auto ts = h.status();
        if (ts.state != lt::torrent_status::downloading) continue;
        std::string hash = to_hex_string(h.info_hash());
        if (m_moov_boosted.count(hash)) continue;
        const int total = h.torrent_file()->num_pieces();
        const int from  = total * 95 / 100;
        for (int i = from; i < total; ++i)
            h.piece_priority(lt::piece_index_t(i), lt::top_priority);
        m_moov_boosted.insert(hash);
    }
}
// Called in HttpServer.cpp broadcaster loop once per second
```

---

### Issue 7 — HEVC codec in Tauri/WebView2
**Problem:** H.265/HEVC content plays fine in standalone Chromium/Chrome (which has
built-in HEVC decode since v107) but fails in Tauri's embedded WebView2 with
`videoHeight=0` after `loadedmetadata`.
**Diagnostic:** `streamRetryCount === 0` + `videoHeight === 0` = moov atom was
served cleanly, codec cannot decode → HEVC issue. `streamRetryCount > 0` = moov
still loading.
**Fix attempted:** Added to `tauri.conf.json`:
```json
"additionalBrowserArgs": "--ignore-certificate-errors --enable-features=PlatformHEVCDecoderSupport,HardwareAccelerationModeDefault"
```
**Status:** Still failing. K-Lite Codec Pack installed (provides Windows MF HEVC filter)
but WebView2 may still not use it.
**Remaining options:**
1. Test if `PlatformHEVCDecoderSupport` + K-Lite works after full reboot
2. Free MS Store codec: `ms-windows-store://pdp/?productid=9n4wgh0z6vhq`
3. Implement "Open in MPV" button in the UI — MPV has its own HEVC decoder

---

## Current State of Each File

| File | Status | Summary of Changes |
|------|--------|--------------------|
| `server/src/core/TorrentEngine.cpp` | ✅ Modified, **NEEDS BUILD** | `waitForPiece` returns bool; moov seek detection with 4s wait and bulk end-piece boost; `proactivelyBoostEndPieces()` implemented |
| `server/include/mediastream/core/TorrentEngine.hpp` | ✅ Modified, **NEEDS BUILD** | `waitForPiece` returns bool; `proactivelyBoostEndPieces()` declared; `m_moov_boosted` set added |
| `server/include/mediastream/api/StreamController.hpp` | ✅ Modified, **NEEDS BUILD** | Returns 503 Retry-After when `waitForPiece` returns false |
| `server/src/api/HttpServer.cpp` | ✅ Modified, **NEEDS BUILD** | Calls `proactivelyBoostEndPieces()` every broadcaster tick |
| `clientTauri/src/routes/watch/[infoHash]/+page.svelte` | ✅ Live (HMR) | Stable `videoSrc` let; state guard for Checking; 20-retry handler; codec vs moov diagnostic |
| `clientTauri/src-tauri/tauri.conf.json` | ✅ Live | HEVC feature flags added |
| Nginx config (on VM) | ✅ Applied | WS upgrade headers; stream proxy_buffering off |

---

## Pending Build Deployment
The backend changes have NOT been built yet. Run on the VM:
```bash
cd ~/cpp-app && git pull
cd server && ./quick-build.sh
sudo systemctl restart mediastream
sudo journalctl -u mediastream -f
```
After restart, the server logs should show:
```
Proactive moov boost: pieces 950-999 queued for hash <hash>
```
This confirms the moov pre-fetch is active.

---

## Key Architecture Notes
- libtorrent sequential download: pieces 0…N in order. End pieces (moov) are normally last.
- `h.have_piece(idx)` returns false during `checking_files` even if data is on disk.
- `h.piece_priority(idx, top_priority)` + `h.set_piece_deadline(idx, 0)` requests out-of-order.
- WebView2 HTML5 `<video>` treats 503 as fatal `MEDIA_ERR_SRC_NOT_SUPPORTED` (code 4).
- Chrome/Chromium has HEVC built-in since v107-108; WebView2 needs `PlatformHEVCDecoderSupport`.
