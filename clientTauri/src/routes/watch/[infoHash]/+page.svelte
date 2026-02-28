<script>
    import { page } from "$app/stores";
    import { goto } from "$app/navigation";
    import { onMount, onDestroy } from "svelte";

    // Use the Svelte store directly to avoid reactive closure scope bugs
    const infoHash = $page.params.infoHash;
    $: videoSrc = isReadyToPlay
        ? `https://192.168.1.37:443/api/v1/stream/${infoHash}`
        : null;

    let videoElement;

    // UI State
    let isReadyToPlay = false; // True when backend has the file created
    let isBuffering = true; // True while waiting for video to load or buffering
    let isPlaying = false;
    let error = null;

    // Torrent State
    let statusInterval;
    let torrentProgress = 0;
    let torrentState = "Connecting to peers...";

    onMount(() => {
        console.log("Started Video Player component for hash:", infoHash);

        // Poll the backend for torrent status every 1.5 seconds
        statusInterval = setInterval(async () => {
            try {
                const res = await fetch(
                    "https://192.168.1.37:443/api/v1/status",
                );
                if (res.ok) {
                    const statusList = await res.json();

                    const myTorrent = statusList.find(
                        (t) =>
                            t.info_hash.toLowerCase() ===
                            infoHash.toLowerCase(),
                    );

                    if (myTorrent) {
                        torrentProgress = myTorrent.progress;
                        torrentState = myTorrent.state;

                        // Only log periodically if not full to avoid spam, or just log once it updates
                        //console.log("progress: ", torrentProgress, "state: " , torrentState);

                        // If progress > 0, it means it finished downloading metadata
                        // and has started writing file pieces to disk.
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
                        console.log(
                            "Backend provided hashes:",
                            statusList.map((t) => t.info_hash),
                        );
                    }
                }
            } catch (e) {
                console.warn("Status fetch failed", e);
            }
        }, 1500);
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

                await fetch("https://192.168.1.37:443/api/v1/user/history", {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify(payload),
                });
            } catch (e) {
                console.error("Failed to POST history:", e);
            }
        }
    }

    onDestroy(() => {
        if (statusInterval) clearInterval(statusInterval);
        saveWatchHistory();
    });

    function goBack() {
        saveWatchHistory();
        goto("/");
    }

    function handleVideoWaiting() {
        isBuffering = true;
    }

    function handleVideoPlaying() {
        isBuffering = false;
        isPlaying = true;
    }

    function handleVideoPause() {
        isPlaying = false;
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

<div class="player-container">
    <!-- Top Bar -->
    <div class="top-bar">
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
    <video
        bind:this={videoElement}
        src={videoSrc}
        controls
        autoplay
        crossorigin="anonymous"
        on:waiting={handleVideoWaiting}
        on:playing={handleVideoPlaying}
        on:pause={handleVideoPause}
        on:error={handleVideoError}
        class="media-video"
    >
        Your browser does not support HTML5 video.
    </video>

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
</style>
