<script>
    import { page } from "$app/stores";
    import { goto } from "$app/navigation";
    import { onMount, onDestroy } from "svelte";

    // Use the Svelte store directly to avoid reactive closure scope bugs
    const infoHash = $page.params.infoHash;
    $: videoSrc = isReadyToPlay
        ? `${import.meta.env.VITE_API_URL}/api/v1/stream/${infoHash}`
        : null;

    let videoElement;

    // UI State
    let isReadyToPlay = false; // True when backend has the file created
    let isBuffering = true; // True while waiting for video to load or buffering
    let error = null;

    // Player State
    let currentTime = 0;
    let duration = 0;
    let isPaused = false;
    let volume = 1;
    let isMuted = false;
    let isControlsVisible = true;
    let controlsTimeout = null;

    // Torrent State
    let ws;
    let isDestroyed = false;
    let torrentProgress = 0;
    let torrentState = "Connecting to peers...";

    function connectWebSocket() {
        ws = new WebSocket(`${import.meta.env.VITE_WS_URL}/api/v1/ws/status`);

        ws.onopen = () => {
            console.log("WebSocket connected for status updates");
        };

        ws.onmessage = (event) => {
            try {
                const statusList = JSON.parse(event.data);
                const myTorrent = statusList.find(
                    (t) => t.info_hash.toLowerCase() === infoHash.toLowerCase(),
                );

                if (myTorrent) {
                    torrentProgress = myTorrent.progress;
                    torrentState = myTorrent.state;

                    if (torrentProgress > 0.05 && !isReadyToPlay) {
                        console.log(
                            "Torrent is ready! Initializing stream.",
                            "progress: ",
                            torrentProgress,
                        );
                        isReadyToPlay = true;
                    }
                } else {
                    console.warn(
                        "Torrent not found in backend status loop! Target Hash:",
                        infoHash,
                    );
                }
            } catch (e) {
                console.warn("Status parse failed", e);
            }
        };

        ws.onclose = () => {
            if (!isDestroyed && !isReadyToPlay) {
                console.log("WebSocket closed prematurely, reconnecting...");
                setTimeout(connectWebSocket, 2000);
            }
        };
    }

    async function fetchInitialStatus() {
        try {
            const res = await fetch(
                `${import.meta.env.VITE_API_URL}/api/v1/status`,
            );
            const statusList = await res.json();
            const myTorrent = statusList.find(
                (t) => t.info_hash.toLowerCase() === infoHash.toLowerCase(),
            );

            if (myTorrent) {
                torrentProgress = myTorrent.progress;
                torrentState = myTorrent.state;
                if (torrentProgress > 0.05 && !isReadyToPlay) {
                    isReadyToPlay = true;
                }
            }
        } catch (e) {
            console.error("Failed to load initial torrent status", e);
        }
    }

    onMount(() => {
        console.log("Started Video Player component for hash:", infoHash);
        fetchInitialStatus();
        connectWebSocket();
    });

    async function saveWatchHistory() {
        if (!videoElement) return;

        const pos = Math.floor(videoElement.currentTime || 0);
        const dur = Math.floor(videoElement.duration || 0);
        if (pos === 0) return; // Didn't watch anything

        const prog = dur > 0 ? pos / dur : 0;
        let targetMedia = null;

        try {
            // Check main discovery cache
            const cacheStr = localStorage.getItem("mediaStreamCache");
            if (cacheStr) {
                const cache = JSON.parse(cacheStr);
                const all = [
                    ...(cache.movies || []),
                    ...(cache.series || []),
                    ...(cache.anime || []),
                ];
                targetMedia = all.find(
                    (m) =>
                        m.torrents &&
                        m.torrents.some(
                            (t) =>
                                t.hash.toLowerCase() === infoHash.toLowerCase(),
                        ),
                );
            }

            // Fallbacks for session storage (history/viewlater rows)
            if (!targetMedia) {
                const histStr = sessionStorage.getItem("historyCache");
                if (histStr) {
                    const hist = JSON.parse(histStr);
                    targetMedia = hist.find(
                        (m) =>
                            m.torrents &&
                            m.torrents.some(
                                (t) =>
                                    t.hash.toLowerCase() ===
                                    infoHash.toLowerCase(),
                            ),
                    );
                }
            }
            if (!targetMedia) {
                const vlStr = sessionStorage.getItem("viewLaterCache");
                if (vlStr) {
                    const vl = JSON.parse(vlStr);
                    targetMedia = vl.find(
                        (m) =>
                            m.torrents &&
                            m.torrents.some(
                                (t) =>
                                    t.hash.toLowerCase() ===
                                    infoHash.toLowerCase(),
                            ),
                    );
                }
            }
        } catch (e) {
            console.error("Failed to read caches for history", e);
        }

        if (targetMedia) {
            try {
                const payload = {
                    media: targetMedia,
                    position_seconds: pos,
                    duration_seconds: dur,
                    progress_percent: prog,
                    completed: prog > 0.95, // Consider complete if > 95%
                };

                await fetch(
                    `${import.meta.env.VITE_API_URL}/api/v1/user/history`,
                    {
                        method: "POST",
                        headers: { "Content-Type": "application/json" },
                        body: JSON.stringify(payload),
                    },
                );
            } catch (e) {
                console.error("Failed to POST history:", e);
            }
        }
    }

    onDestroy(() => {
        isDestroyed = true;
        if (ws) {
            ws.onclose = null; // Prevent reconnect
            ws.close();
        }
        localStorage.setItem("mediaStream_history_dirty", "true");
        saveWatchHistory();
    });

    function goBack() {
        saveWatchHistory();
        localStorage.setItem("mediaStream_history_dirty", "true");
        goto("/");
    }

    function handleVideoWaiting() {
        isBuffering = true;
    }

    function handleVideoPlaying() {
        isBuffering = false;
    }

    function handleVideoPause() {
        // Handled by bind:paused
    }

    function showControls() {
        isControlsVisible = true;
        clearTimeout(controlsTimeout);
        controlsTimeout = setTimeout(() => {
            if (!isPaused) isControlsVisible = false;
        }, 3000);
    }

    function togglePlay() {
        if (!videoElement) return;
        if (isPaused) videoElement.play();
        else videoElement.pause();
    }

    function toggleMute() {
        isMuted = !isMuted;
    }

    function formatTime(secs) {
        if (isNaN(secs) || secs < 0) return "00:00";
        const h = Math.floor(secs / 3600);
        const m = Math.floor((secs % 3600) / 60);
        const s = Math.floor(secs % 60);
        return h > 0
            ? `${h}:${m.toString().padStart(2, "0")}:${s.toString().padStart(2, "0")}`
            : `${m.toString().padStart(2, "0")}:${s.toString().padStart(2, "0")}`;
    }

    function handleVideoError(e) {
        console.error("Video Error:", e);
        // HTML5 video errors often happen if the browser tries to read the stream
        // before enough of the moov atom / header is downloaded.

        // Try to recover by reloading a bit later
        setTimeout(() => {
            if (videoElement && isReadyToPlay) {
                videoElement.load();
                videoElement.play().catch(() => {});
            }
        }, 3000);
    }
</script>

<!-- svelte-ignore a11y-no-static-element-interactions -->
<div
    class="player-container"
    on:mousemove={showControls}
    on:mouseleave={() => (isControlsVisible = false)}
>
    <!-- Top Bar -->
    <div class="top-bar" class:hidden={!isControlsVisible && !isPaused}>
        <button class="back-btn" on:click={goBack}>
            <svg
                xmlns="http://www.w3.org/2005/svg"
                width="24"
                height="24"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                stroke-width="2"
                stroke-linecap="round"
                stroke-linejoin="round"
                ><line x1="19" y1="12" x2="5" y2="12"></line><polyline
                    points="12 19 5 12 12 5"
                ></polyline></svg
            >
            Back to Browse
        </button>
        <div class="session-info">
            {#if torrentProgress > 0}
                <span class="progress-badge"
                    >Buffer: {(torrentProgress * 100).toFixed(1)}%</span
                >
            {/if}
        </div>
    </div>

    <!-- Video Element -->
    <!-- svelte-ignore a11y-media-has-caption -->
    <!-- svelte-ignore a11y-click-events-have-key-events -->
    <video
        bind:this={videoElement}
        bind:currentTime
        bind:duration
        bind:paused={isPaused}
        bind:volume
        bind:muted={isMuted}
        src={videoSrc}
        autoplay
        crossorigin="anonymous"
        on:waiting={handleVideoWaiting}
        on:playing={handleVideoPlaying}
        on:click={togglePlay}
        on:error={handleVideoError}
        class="media-video"
    >
        Your browser does not support HTML5 video.
    </video>

    <!-- Bottom Controls Overlay -->
    <div
        class="controls-overlay"
        class:hidden={!isControlsVisible && !isPaused}
    >
        <!-- Timeline -->
        <div class="timeline-container">
            <span class="time-read">{formatTime(currentTime)}</span>
            <input
                type="range"
                class="timeline-slider"
                min="0"
                max={duration || 1}
                bind:value={currentTime}
                on:mousedown={() => {
                    if (!isPaused && videoElement) videoElement.pause();
                }}
                on:mouseup={() => {
                    if (isPaused && videoElement) videoElement.play();
                }}
                on:input={() => {
                    if (videoElement) videoElement.currentTime = currentTime;
                }}
            />
            <span class="time-read">{formatTime(duration)}</span>
        </div>

        <div class="controls-row">
            <div class="controls-left">
                <!-- Play/Pause -->
                <button class="control-btn" on:click={togglePlay}>
                    {#if isPaused}
                        <svg
                            viewBox="0 0 24 24"
                            fill="currentColor"
                            width="28"
                            height="28"
                        >
                            <polygon points="5 3 19 12 5 21 5 3"></polygon>
                        </svg>
                    {:else}
                        <svg
                            viewBox="0 0 24 24"
                            fill="currentColor"
                            width="28"
                            height="28"
                        >
                            <rect x="6" y="4" width="4" height="16"></rect>
                            <rect x="14" y="4" width="4" height="16"></rect>
                        </svg>
                    {/if}
                </button>

                <!-- Volume Control -->
                <div class="volume-group">
                    <button class="control-btn" on:click={toggleMute}>
                        {#if isMuted || volume === 0}
                            <svg
                                viewBox="0 0 24 24"
                                fill="none"
                                stroke="currentColor"
                                stroke-width="2"
                                stroke-linecap="round"
                                stroke-linejoin="round"
                                width="24"
                                height="24"
                            >
                                <polygon
                                    points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"
                                ></polygon>
                                <line x1="23" y1="9" x2="17" y2="15"
                                ></line><line x1="17" y1="9" x2="23" y2="15"
                                ></line>
                            </svg>
                        {:else}
                            <svg
                                viewBox="0 0 24 24"
                                fill="none"
                                stroke="currentColor"
                                stroke-width="2"
                                stroke-linecap="round"
                                stroke-linejoin="round"
                                width="24"
                                height="24"
                            >
                                <polygon
                                    points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"
                                ></polygon>
                                <path
                                    d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"
                                ></path>
                            </svg>
                        {/if}
                    </button>
                    <input
                        type="range"
                        class="volume-slider"
                        min="0"
                        max="1"
                        step="0.05"
                        bind:value={volume}
                    />
                </div>
            </div>

            <div class="controls-right">
                <!-- Subtitles Button Dummy -->
                <button class="control-btn config-btn">
                    <svg
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        width="22"
                        height="22"
                    >
                        <path
                            d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"
                        ></path>
                        <line x1="9" y1="9" x2="15" y2="9"></line>
                        <line x1="9" y1="13" x2="11" y2="13"></line>
                    </svg>
                    <span>Subtitles</span>
                </button>
            </div>
        </div>
    </div>

    <!-- Overlays -->
    {#if !isReadyToPlay || (isBuffering && !error)}
        <div class="overlay buffering-overlay">
            <div class="spinner"></div>
            <p>
                {!isReadyToPlay
                    ? `Establishing connection... (${torrentState})`
                    : "Buffering media stream..."}
            </p>
        </div>
    {/if}

    {#if error}
        <div class="overlay error-overlay">
            <div class="spinner"></div>
            <p>{error}</p>
        </div>
    {/if}
</div>

<style>
    .player-container {
        position: fixed;
        top: 0;
        left: 0;
        width: 100vw;
        height: 100vh;
        background-color: #000;
        z-index: 2000;
        display: flex;
        flex-direction: column;
    }

    .top-bar {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        padding: var(--spacing-md) var(--spacing-xl);
        display: flex;
        justify-content: space-between;
        align-items: center;
        background: linear-gradient(
            180deg,
            rgba(0, 0, 0, 0.8) 0%,
            transparent 100%
        );
        z-index: 2010;
        transition: opacity var(--transition-normal);
    }

    /* Hide top bar when playing and not hovering, handled by native controls usually, but we keep it simple here */
    .player-container:not(:hover) .top-bar {
        opacity: 0.8;
    }

    .back-btn {
        background: rgba(20, 20, 20, 0.6);
        color: white;
        border: 1px solid rgba(255, 255, 255, 0.2);
        padding: 8px 16px;
        border-radius: var(--border-radius-sm);
        display: flex;
        align-items: center;
        gap: 8px;
        font-weight: 600;
        backdrop-filter: blur(10px);
        transition: all var(--transition-fast);
    }

    .back-btn:hover {
        background: white;
        color: black;
        transform: scale(1.05);
    }

    .progress-badge {
        background: rgba(229, 9, 20, 0.2);
        color: var(--accent-color);
        border: 1px solid var(--accent-color);
        padding: 4px 12px;
        border-radius: 20px;
        font-size: 0.9rem;
        font-weight: bold;
    }

    .media-video {
        width: 100%;
        height: 100%;
        object-fit: contain;
        background-color: #000;
        outline: none;
    }

    .overlay {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.6);
        backdrop-filter: blur(10px);
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        z-index: 2005;
        color: white;
        font-size: 1.2rem;
    }

    .error-overlay {
        background-color: rgba(0, 0, 0, 0.85);
    }

    .spinner {
        width: 60px;
        height: 60px;
        border: 4px solid rgba(255, 255, 255, 0.1);
        border-top: 4px solid var(--accent-color);
        border-radius: 50%;
        animation: spin 1s linear infinite;
        margin-bottom: 20px;
    }

    @keyframes spin {
        0% {
            transform: rotate(0deg);
        }
        100% {
            transform: rotate(360deg);
        }
    }

    /* --- Custom Player Controls --- */
    .hidden {
        opacity: 0 !important;
        pointer-events: none;
    }

    .controls-overlay {
        position: absolute;
        bottom: 0;
        left: 0;
        width: 100%;
        padding: var(--spacing-xxl) var(--spacing-xl) var(--spacing-xl)
            var(--spacing-xl);
        background: linear-gradient(
            0deg,
            rgba(0, 0, 0, 0.9) 0%,
            transparent 100%
        );
        display: flex;
        flex-direction: column;
        gap: var(--spacing-md);
        z-index: 2010;
        transition: opacity var(--transition-normal);
    }

    .timeline-container {
        display: flex;
        align-items: center;
        gap: var(--spacing-md);
        width: 100%;
    }

    .time-read {
        font-family: monospace;
        font-size: 0.95rem;
        color: #ddd;
        min-width: 60px;
        text-align: center;
    }

    .timeline-slider {
        flex: 1;
        cursor: pointer;
        height: 6px;
        border-radius: 3px;
        accent-color: var(--accent-color);
        background: rgba(255, 255, 255, 0.2);
    }

    .controls-row {
        display: flex;
        justify-content: space-between;
        align-items: center;
        width: 100%;
    }

    .controls-left,
    .controls-right {
        display: flex;
        align-items: center;
        gap: var(--spacing-lg);
    }

    .control-btn {
        background: transparent;
        border: none;
        color: white;
        cursor: pointer;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 8px;
        border-radius: 50%;
        transition: all var(--transition-fast);
        opacity: 0.8;
    }

    .control-btn:hover {
        opacity: 1;
        background: rgba(255, 255, 255, 0.15);
        transform: scale(1.1);
    }

    .volume-group {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .volume-slider {
        width: 80px;
        cursor: pointer;
        accent-color: white;
        height: 4px;
        opacity: 0;
        transition: opacity var(--transition-fast);
    }

    .volume-group:hover .volume-slider {
        opacity: 1;
    }

    .config-btn {
        border-radius: var(--border-radius-sm);
        padding: 8px 16px;
        gap: 8px;
        font-weight: 500;
        font-size: 0.9rem;
    }

    .config-btn:hover {
        background: rgba(255, 255, 255, 0.2);
    }
</style>
