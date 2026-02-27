<script>
    import { onMount } from 'svelte';
    import { goto } from '$app/navigation';
    import PosterCard from '../lib/PosterCard.svelte';
    import MediaCard from '../lib/MediaCard.svelte';

    let history = [];
    let watchLater = [];
    let movies = [];
    let series = [];
    let anime = [];

    let selectedMedia = null;

    onMount(async () => {
        // Fetch movies
        try {
            const moviesRes = await fetch('https://192.168.1.37:443/api/v1/discover/movies');
            if (moviesRes.ok) movies = await moviesRes.json();
            
            const seriesRes = await fetch('https://192.168.1.37:443/api/v1/discover/series');
            if (seriesRes.ok) series = await seriesRes.json();
            
            const animeRes = await fetch('https://192.168.1.37:443/api/v1/discover/anime');
            if (animeRes.ok) anime = await animeRes.json();
        } catch (e) {
            console.error("Failed to fetch discovery lists. Is the C++ Server running?", e);
        }
    });

    function openMediaCard(event) {
        selectedMedia = event.detail;
        document.body.style.overflow = 'hidden'; // Prevent page scrolling under modal
    }

    function closeMediaCard() {
        selectedMedia = null;
        document.body.style.overflow = 'auto';
    }

    async function handlePlay(event) {
        const { media, torrent } = event.detail;
        console.log("Playing:", media.title, "Torrent:", torrent.hash);
        
        try {
            const res = await fetch('https://192.168.1.37:443/api/v1/torrents', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ magnet_link: torrent.magnet_uri })
            });

            if (res.ok) {
                // Torrent added successfully, go to video player
                goto(`/watch/${torrent.hash}`);
            } else {
                const err = await res.json();
                alert(`Error starting stream: ${err.message || res.statusText}`);
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
            const res = await fetch('https://192.168.1.37:443/api/v1/torrents', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ magnet_link: torrent.magnet_uri })
            });

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
        <!-- SVG Search Icon -->
        <button class="icon-btn" aria-label="Search">
            <svg xmlns="http://www.w3.org/2005/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"></circle><line x1="21" y1="21" x2="16.65" y2="16.65"></line></svg>
        </button>
        <!-- SVG Settings Icon -->
        <button class="icon-btn" aria-label="Settings">
            <svg xmlns="http://www.w3.org/2005/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path></svg>
        </button>
    </div>
</header>

<!-- MAIN CONTENT -->
<main class="gallery-container">
    
    <!-- Watch History -->
    <section class="gallery-row">
        <h2>Watch History</h2>
        <div class="row-scroll hide-scroll">
            {#if history.length === 0}
                <div class="empty-state">No watch history yet.</div>
            {:else}
                {#each history as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
    </section>

    <!-- View Later -->
    <section class="gallery-row">
        <h2>View Later</h2>
        <div class="row-scroll hide-scroll">
            {#if watchLater.length === 0}
                <div class="empty-state">Your list is empty.</div>
            {:else}
                {#each watchLater as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
    </section>

    <!-- Popular Movies -->
    <section class="gallery-row">
        <h2>Popular Movies</h2>
        <div class="row-scroll hide-scroll">
            {#if movies.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each movies as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
    </section>

    <!-- Popular Series -->
    <section class="gallery-row">
        <h2>Popular Series (EZTV)</h2>
        <div class="row-scroll hide-scroll">
            {#if series.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each series as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
    </section>

    <!-- Popular Anime -->
    <section class="gallery-row">
        <h2>Popular Anime (Nyaa)</h2>
        <div class="row-scroll hide-scroll">
            {#if anime.length === 0}
                <div class="empty-state spinner"></div>
            {:else}
                {#each anime as item}
                    <PosterCard {item} on:select={openMediaCard} />
                {/each}
            {/if}
        </div>
    </section>

</main>

{#if selectedMedia}
    <MediaCard item={selectedMedia} on:close={closeMediaCard} on:play={handlePlay} on:download={handleDownload} />
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
        box-shadow: 0 2px 10px rgba(0,0,0,0.5);
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

    .nav-links li:hover, .nav-links li.active {
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
        transition: transform var(--transition-fast), color var(--transition-fast);
        padding: var(--spacing-sm);
    }
    .icon-btn:hover {
        transform: scale(1.1);
        color: #CCC;
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
        color: #E5E5E5;
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

    .spinner {
        width: 40px;
        height: 40px;
        border: 4px solid rgba(255,255,255,0.1);
        border-top: 4px solid var(--accent-color);
        border-radius: 50%;
        animation: spin 1s linear infinite;
        margin: 20px auto;
        background-color: transparent;
    }

    @keyframes spin {
        0% { transform: rotate(0deg); }
        100% { transform: rotate(360deg); }
    }
</style>
