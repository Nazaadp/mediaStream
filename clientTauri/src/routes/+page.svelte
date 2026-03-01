<script>
    import { onMount } from "svelte";
    import { goto } from "$app/navigation";
    import PosterCard from "../lib/PosterCard.svelte";
    import MediaCard from "../lib/MediaCard.svelte";

    let history = [];
    let watchLater = [];
    let movies = [];
    let series = [];
    let anime = [];

    let selectedMedia = null;

    let isRefreshing = false;

    async function refreshData() {
        if (isRefreshing) return;
        isRefreshing = true;

        // Clear current items to show loading spinners
        history = [];
        watchLater = [];
        movies = [];
        series = [];
        anime = [];

        try {
            const [histRes, vlRes, moviesRes, seriesRes, animeRes] =
                await Promise.all([
                    fetch("https://192.168.1.37:443/api/v1/user/history"),
                    fetch("https://192.168.1.37:443/api/v1/user/viewlater"),
                    fetch("https://192.168.1.37:443/api/v1/discover/movies"),
                    fetch("https://192.168.1.37:443/api/v1/discover/series"),
                    fetch("https://192.168.1.37:443/api/v1/discover/anime"),
                ]);

            if (histRes.ok) history = await histRes.json();
            if (vlRes.ok) watchLater = await vlRes.json();
            if (moviesRes.ok) movies = await moviesRes.json();
            if (seriesRes.ok) series = await seriesRes.json();
            if (animeRes.ok) anime = await animeRes.json();

            // Save user stuff to sessionStorage
            sessionStorage.setItem("historyCache", JSON.stringify(history));
            sessionStorage.setItem(
                "viewLaterCache",
                JSON.stringify(watchLater),
            );

            // Save discovery to localStorage
            const cacheData = {
                movies,
                series,
                anime,
                timestamp: Date.now(),
            };
            localStorage.setItem("mediaStreamCache", JSON.stringify(cacheData));
        } catch (e) {
            console.error("Failed to fetch data.", e);
        } finally {
            isRefreshing = false;
        }
    }

    onMount(() => {
        // Try to load history and View Later first
        try {
            let histCache = sessionStorage.getItem("historyCache");
            if (histCache) history = JSON.parse(histCache);

            let vlCache = sessionStorage.getItem("viewLaterCache");
            if (vlCache) watchLater = JSON.parse(vlCache);
        } catch (e) {}

        // Try to load discovery from cache first
        const cachedStr = localStorage.getItem("mediaStreamCache");
        if (cachedStr) {
            try {
                const cacheData = JSON.parse(cachedStr);
                movies = cacheData.movies || [];
                series = cacheData.series || [];
                anime = cacheData.anime || [];
                return; // Use cached data
            } catch (e) {
                console.warn("Failed to parse cache", e);
            }
        }

        // If no cache or error parsing, fetch fresh data
        refreshData();
    });

    function openMediaCard(event) {
        selectedMedia = event.detail;
        document.body.style.overflow = "hidden"; // Prevent page scrolling under modal
    }

    function closeMediaCard() {
        selectedMedia = null;
        document.body.style.overflow = "auto";
        // Immediately sync changes made in the modal back into the reactive lists
        try {
            let vlCache = sessionStorage.getItem("viewLaterCache");
            if (vlCache) watchLater = JSON.parse(vlCache);

            let histCache = sessionStorage.getItem("historyCache");
            if (histCache) history = JSON.parse(histCache);
        } catch (e) {}
    }

    function scrollRow(event, direction) {
        // Navigates up to the .gallery-row, then finds the inner .row-scroll container
        const row = event.currentTarget
            .closest(".gallery-row")
            .querySelector(".row-scroll");
        if (row) {
            const scrollAmount = row.clientWidth * 0.75; // Scroll 75% of the visible width
            row.scrollBy({
                left: direction * scrollAmount,
                behavior: "smooth",
            });
        }
    }

    async function handlePlay(event) {
        const { media, torrent } = event.detail;
        console.log("Playing:", media.title, "Torrent:", torrent.hash);

        try {
            const res = await fetch(
                "https://192.168.1.37:443/api/v1/torrents",
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json",
                    },
                    body: JSON.stringify({ magnet_link: torrent.magnet_uri }),
                },
            );

            if (res.ok) {
                // Torrent added successfully, go to video player
                goto(`/watch/${torrent.hash}`);
            } else {
                const err = await res.json();
                alert(
                    `Error starting stream: ${err.message || res.statusText}`,
                );
            }
        } catch (e) {
            console.error("Failed to start stream:", e);
            alert("Failed to connect to the backend securely.");
        }
    }

    async function handleDownload(event) {
        const { media, torrent } = event.detail;
        console.log("Downloading:", media.title, "Torrent:", torrent.hash);

        try {
            const res = await fetch(
                "https://192.168.1.37:443/api/v1/torrents",
                {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({ magnet_link: torrent.magnet_uri }),
                },
            );

            if (res.ok) {
                // The MediaCard will handle refreshing its own state natively
            } else {
                alert(`Error starting download: ${res.statusText}`);
            }
        } catch (e) {
            console.error("Failed to start download:", e);
            alert("Failed to connect to the backend securely.");
        }
    }
</script>

<!-- HEADER -->
<header class="navbar">
    <div class="logo">MediaStream</div>
    <ul class="nav-links">
        <li class="active">Home</li>
        <li>Movies</li>
        <li>Series</li>
        <li>Anime</li>
    </ul>
    <div class="nav-actions">
        <!-- SVG Refresh Icon -->
        <button
            class="icon-btn"
            aria-label="Refresh"
            title="Refresh Discovery Lists"
            on:click={refreshData}
            class:spinning={isRefreshing}
        >
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
                ><polyline points="23 4 23 10 17 10"></polyline><polyline
                    points="1 20 1 14 7 14"
                ></polyline><path
                    d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"
                ></path></svg
            >
        </button>
        <!-- SVG Search Icon -->
        <button class="icon-btn" aria-label="Search">
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
                ><circle cx="11" cy="11" r="8"></circle><line
                    x1="21"
                    y1="21"
                    x2="16.65"
                    y2="16.65"
                ></line></svg
            >
        </button>
        <!-- SVG Settings Icon -->
        <button class="icon-btn" aria-label="Settings">
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
                ><circle cx="12" cy="12" r="3"></circle><path
                    d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"
                ></path></svg
            >
        </button>
    </div>
</header>

<!-- MAIN CONTENT -->
<main class="gallery-container">
    <!-- Watch History -->
    <section class="gallery-row">
        <h2>Watch History</h2>
        {#if history.length > 7}
            <button class="scroll-btn left" on:click={(e) => scrollRow(e, -1)}
                >&#10094;</button
            >
        {/if}
        <div class="row-scroll hide-scroll">
            {#if history.length === 0}
                <div class="empty-state">No watch history yet.</div>
            {:else}
                {#each history as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
        {#if history.length > 7}
            <button class="scroll-btn right" on:click={(e) => scrollRow(e, 1)}
                >&#10095;</button
            >
        {/if}
    </section>

    <!-- View Later -->
    <section class="gallery-row">
        <h2>View Later</h2>
        {#if watchLater.length > 7}
            <button class="scroll-btn left" on:click={(e) => scrollRow(e, -1)}
                >&#10094;</button
            >
        {/if}
        <div class="row-scroll hide-scroll">
            {#if watchLater.length === 0}
                <div class="empty-state">Your list is empty.</div>
            {:else}
                {#each watchLater as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
        {#if watchLater.length > 7}
            <button class="scroll-btn right" on:click={(e) => scrollRow(e, 1)}
                >&#10095;</button
            >
        {/if}
    </section>

    <!-- Popular Movies -->
    <section class="gallery-row">
        <h2>Popular Movies</h2>
        {#if movies.length > 7}
            <button class="scroll-btn left" on:click={(e) => scrollRow(e, -1)}
                >&#10094;</button
            >
        {/if}
        <div class="row-scroll hide-scroll">
            {#if movies.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each movies as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
        {#if movies.length > 7}
            <button class="scroll-btn right" on:click={(e) => scrollRow(e, 1)}
                >&#10095;</button
            >
        {/if}
    </section>

    <!-- Popular Series -->
    <section class="gallery-row">
        <h2>Popular Series (EZTV)</h2>
        {#if series.length > 7}
            <button class="scroll-btn left" on:click={(e) => scrollRow(e, -1)}
                >&#10094;</button
            >
        {/if}
        <div class="row-scroll hide-scroll">
            {#if series.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each series as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
        {#if series.length > 7}
            <button class="scroll-btn right" on:click={(e) => scrollRow(e, 1)}
                >&#10095;</button
            >
        {/if}
    </section>

    <!-- Popular Anime -->
    <section class="gallery-row">
        <h2>Popular Anime (Nyaa)</h2>
        {#if anime.length > 7}
            <button class="scroll-btn left" on:click={(e) => scrollRow(e, -1)}
                >&#10094;</button
            >
        {/if}
        <div class="row-scroll hide-scroll">
            {#if anime.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each anime as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
        {#if anime.length > 7}
            <button class="scroll-btn right" on:click={(e) => scrollRow(e, 1)}
                >&#10095;</button
            >
        {/if}
    </section>
</main>

{#if selectedMedia}
    <MediaCard
        item={selectedMedia}
        on:close={closeMediaCard}
        on:play={handlePlay}
        on:download={handleDownload}
    />
{/if}

<style>
    .navbar {
        position: fixed;
        top: 0;
        width: 100%;
        height: 70px;
        background-color: var(--nav-bg);
        display: flex;
        align-items: center;
        padding: 0 var(--spacing-xl);
        z-index: 100;
        backdrop-filter: blur(10px);
        box-shadow: 0 2px 10px rgba(0, 0, 0, 0.5);
    }

    .logo {
        color: var(--accent-color);
        font-size: 1.5rem;
        font-weight: 700;
        text-transform: uppercase;
        letter-spacing: 1px;
        margin-right: var(--spacing-xl);
    }

    .nav-links {
        display: flex;
        list-style: none;
        gap: var(--spacing-lg);
        flex-grow: 1;
    }

    .nav-links li {
        color: var(--text-muted);
        font-weight: 500;
        cursor: pointer;
        transition: color var(--transition-fast);
        font-size: 0.95rem;
    }

    .nav-links li:hover,
    .nav-links li.active {
        color: var(--text-main);
    }

    .nav-links li.active {
        font-weight: 600;
    }

    .nav-actions {
        display: flex;
        gap: var(--spacing-md);
    }

    .icon-btn {
        color: var(--text-main);
        transition:
            transform var(--transition-fast),
            color var(--transition-fast);
        padding: var(--spacing-sm);
    }
    .icon-btn:hover {
        transform: scale(1.1);
        color: #ccc;
    }

    .icon-btn.spinning svg {
        animation: spin 1s linear infinite;
    }

    .gallery-container {
        padding: 90px var(--spacing-xl) var(--spacing-xl) var(--spacing-xl);
        min-height: 100vh;
    }

    .gallery-row {
        margin-bottom: var(--spacing-xl);
        position: relative;
    }

    .gallery-row h2 {
        font-size: 1.4rem;
        margin-bottom: var(--spacing-sm);
        margin-left: 10px;
        color: #e5e5e5;
        font-weight: 600;
        letter-spacing: 0.5px;
    }

    .row-scroll {
        display: flex;
        overflow-x: auto;
        padding: 20px 10px; /* Space for hover scale */
        gap: var(--spacing-sm);
        scroll-behavior: smooth;
    }

    .empty-state {
        color: var(--text-muted);
        padding: var(--spacing-lg);
        background-color: #1a1a1a;
        border-radius: var(--border-radius-md);
        display: flex;
        align-items: center;
        width: 100%;
    }

    .scroll-btn {
        position: absolute;
        top: 50%;
        transform: translateY(-50%);
        width: 50px;
        height: 50px;
        border-radius: 50%;
        background: rgba(0, 0, 0, 0.6);
        color: white;
        border: 2px solid rgba(255, 255, 255, 0.2);
        font-size: 1.5rem;
        cursor: pointer;
        opacity: 0;
        transition:
            opacity 0.3s ease,
            background 0.3s ease,
            transform 0.2s ease;
        z-index: 50;
        display: flex;
        align-items: center;
        justify-content: center;
        backdrop-filter: blur(4px);
    }

    .scroll-btn:hover {
        background: rgba(0, 0, 0, 0.9);
        border-color: white;
        transform: translateY(-50%) scale(1.1);
    }

    .gallery-row:hover .scroll-btn {
        opacity: 1;
    }

    .scroll-btn.left {
        left: 20px;
    }

    .scroll-btn.right {
        right: 20px;
    }

    .spinner {
        width: 40px;
        height: 40px;
        border: 4px solid rgba(255, 255, 255, 0.1);
        border-top: 4px solid var(--accent-color);
        border-radius: 50%;
        animation: spin 1s linear infinite;
        margin: 20px auto;
        background-color: transparent;
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
