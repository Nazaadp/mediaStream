<script>
    export let item;
    let activeDownloads = {};
    let statusInterval;

    import { createEventDispatcher, onMount, onDestroy } from 'svelte';
    const dispatch = createEventDispatcher();

    function close() {
        dispatch('close');
    }

    async function fetchActiveDownloads() {
        try {
            const res = await fetch('https://192.168.1.37:443/api/v1/status');
            if (res.ok) {
                const statusList = await res.json();
                let newDownloads = {};
                for (const t of statusList) {
                    newDownloads[t.info_hash.toLowerCase()] = {
                        progress: t.progress,
                        state: t.state
                    };
                }
                activeDownloads = newDownloads;
            }
        } catch (e) {}
    }

    onMount(() => {
        fetchActiveDownloads(); // Fetch immediately upon opening modal
        statusInterval = setInterval(fetchActiveDownloads, 1500); // 1.5s refresh for smooth pie chart
    });

    onDestroy(() => {
        if (statusInterval) clearInterval(statusInterval);
    });

    // Default season/episode selects for TV Series/Anime
    let selectedSeason = 1;
    let selectedEpisode = 1;

    $: isMovie = item.source === 'YTS' || item.type === 'movie';

    // Helper for SVG circle dash array math (circumference is ~88 for r=14)
    function getDashOffset(progress) {
        const circumference = 2 * Math.PI * 14;
        return circumference - (progress * circumference);
    }
</script>

<!-- svelte-ignore a11y-click-events-have-key-events a11y-no-static-element-interactions -->
<div class="modal-backdrop" on:click={close}>
    <div class="modal-content" on:click|stopPropagation>
        <button class="close-btn" on:click={close}>&times;</button>
        
        <!-- Backdrop/Header Banner -->
        <div class="banner">
            <div class="banner-gradient"></div>
            <img class="backdrop-img" src={item.backdrop_url || item.poster_url || 'https://via.placeholder.com/1200x600'} alt="Backdrop" />
            <div class="banner-info">
                <h1 class="target-title">{item.title}</h1>
                <div class="meta">
                    <span class="year">{item.year || 'Unknown'}</span>
                    <span class="rating">{(item.rating || 0).toFixed(1)} <span class="star">★</span></span>
                    {#if item.genres}
                        <span class="genres">{item.genres}</span>
                    {/if}
                </div>
            </div>
        </div>

        <div class="details">
            <p class="description">{item.description || 'No description available for this title.'}</p>
            
            <div class="interactive-section">
                <!-- If it's a series or anime, show season selector -->
                {#if !isMovie}
                    <div class="season-selector">
                        <h3>Select Season & Episode</h3>
                        <!-- In the future, this will be populated dynamically by the backend -->
                        <div class="selects">
                            <select bind:value={selectedSeason}>
                                <option value={1}>Season 1</option>
                            </select>
                            <select bind:value={selectedEpisode}>
                                <option value={1}>Episode 1</option>
                            </select>
                        </div>
                    </div>
                {/if}

                <h3>{isMovie ? 'Available Torrents' : `Available Torrents (S${selectedSeason}E${selectedEpisode})`}</h3>
                {#if item.torrents && item.torrents.length > 0}
                    <div class="torrents-list">
                        <!-- Header Row -->
                        <div class="torrent-row header">
                            <span>Quality</span>
                            <span>Type</span>
                            <span>Size</span>
                            <span>Peers</span>
                            <span>Action</span>
                        </div>
                        {#each item.torrents as t}
                            <div class="torrent-row">
                                <span class="quality">{t.quality}</span>
                                <span class="type">{t.type}</span>
                                <span class="size">{(t.size_bytes / 1024 / 1024).toFixed(0)} MB</span>
                                <span class="peers">
                                    <span class="seeders">↑ {t.seeders}</span> 
                                    <span class="leechers">↓ {t.leechers}</span>
                                </span>
                                <div class="action-buttons">
                                    {#if activeDownloads && activeDownloads[t.hash.toLowerCase()]}
                                        {#if activeDownloads[t.hash.toLowerCase()].progress >= 0.999}
                                            <div class="progress-indicator done" title="Download Complete">
                                                <svg style="transform: rotate(0deg);" xmlns="http://www.w3.org/2005/svg" width="28" height="28" viewBox="0 0 24 24" fill="#4CAF50" stroke="#4CAF50" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                                                    <circle cx="12" cy="12" r="10" stroke="none"></circle>
                                                    <polyline points="17 9 10 16 7 13" stroke="white" stroke-width="2.5"></polyline>
                                                </svg>
                                            </div>
                                        {:else}
                                            <div class="progress-indicator" title={activeDownloads[t.hash.toLowerCase()].state}>
                                                <svg width="32" height="32" viewBox="0 0 32 32">
                                                    <circle class="bg" cx="16" cy="16" r="14" fill="none" stroke="#333" stroke-width="3" />
                                                    <circle class="bar" cx="16" cy="16" r="14" fill="none" stroke="#4CAF50" stroke-width="3" stroke-dasharray="87.96" stroke-dashoffset={getDashOffset(activeDownloads[t.hash.toLowerCase()].progress)} style="transition: stroke-dashoffset 1.5s ease;" />
                                                </svg>
                                            </div>
                                        {/if}
                                    {:else}
                                        <button class="icon-btn" title="Download to Backend" on:click={() => dispatch('download', { media: item, torrent: t })}>
                                            <svg xmlns="http://www.w3.org/2005/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
                                        </button>
                                    {/if}
                                    <button class="play-btn" on:click={() => dispatch('play', { media: item, torrent: t })}>▶ Stream</button>
                                </div>
                            </div>
                        {/each}
                    </div>
                {:else}
                    <p class="error">No torrents available.</p>
                {/if}
            </div>
        </div>
    </div>
</div>

<style>
    .modal-backdrop {
        position: fixed;
        top: 0;
        left: 0;
        width: 100vw;
        height: 100vh;
        background-color: var(--overlay-bg);
        z-index: 1000;
        display: flex;
        justify-content: center;
        align-items: center;
        backdrop-filter: blur(5px);
    }

    .modal-content {
        background-color: var(--modal-bg);
        width: 70%;
        max-width: 1200px;
        height: 85vh;
        border-radius: var(--border-radius-lg);
        overflow-y: auto;
        position: relative;
        box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 1);
        display: flex;
        flex-direction: column;
        animation: modalFadeIn var(--transition-normal);
    }

    @keyframes modalFadeIn {
        from { opacity: 0; transform: scale(0.95); }
        to { opacity: 1; transform: scale(1); }
    }

    .close-btn {
        position: absolute;
        top: var(--spacing-md);
        right: var(--spacing-md);
        z-index: 10;
        width: 40px;
        height: 40px;
        border-radius: 50%;
        background-color: rgba(20, 20, 20, 0.7);
        color: white;
        font-size: 24px;
        display: flex;
        align-items: center;
        justify-content: center;
        transition: background-color var(--transition-fast);
        border: 2px solid transparent;
    }
    .close-btn:hover {
        background-color: white;
        color: black;
    }

    .banner {
        position: relative;
        width: 100%;
        height: 50vh;
        min-height: 350px;
        flex-shrink: 0;
    }

    .backdrop-img {
        width: 100%;
        height: 100%;
        object-fit: cover;
        object-position: top;
    }

    .banner-gradient {
        position: absolute;
        bottom: 0;
        left: 0;
        width: 100%;
        height: 60%;
        background: linear-gradient(0deg, var(--modal-bg) 0%, transparent 100%);
    }

    .banner-info {
        position: absolute;
        bottom: 0;
        left: 0;
        padding: var(--spacing-xl);
        width: 100%;
    }

    .target-title {
        font-size: 3rem;
        margin-bottom: var(--spacing-sm);
        text-shadow: 2px 2px 4px rgba(0,0,0,0.8);
    }

    .meta {
        display: flex;
        gap: var(--spacing-lg);
        font-size: 1.1rem;
        color: #CCC;
        text-shadow: 1px 1px 2px rgba(0,0,0,0.8);
    }

    .star { color: #f5c518; }
    
    .details {
        padding: 0 var(--spacing-xl) var(--spacing-xl) var(--spacing-xl);
    }

    .description {
        font-size: 1.1rem;
        line-height: 1.6;
        color: #DDD;
        margin-top: var(--spacing-md);
        margin-bottom: var(--spacing-xl);
        max-width: 80%;
    }

    .interactive-section h3 {
        color: var(--text-muted);
        text-transform: uppercase;
        letter-spacing: 1px;
        font-size: 0.9rem;
        margin-bottom: var(--spacing-md);
        margin-top: var(--spacing-lg);
    }

    .season-selector {
        margin-bottom: var(--spacing-xl);
    }

    .selects select {
        padding: 10px 15px;
        background-color: #333;
        color: white;
        border: 1px solid #444;
        border-radius: var(--border-radius-sm);
        margin-right: var(--spacing-md);
        font-size: 1rem;
        outline: none;
        cursor: pointer;
    }

    /* Torrents List */
    .torrents-list {
        background-color: rgba(10, 10, 10, 0.8);
        border: 1px solid #222;
        border-radius: var(--border-radius-sm);
        padding: var(--spacing-sm);
        margin-bottom: var(--spacing-xxl);
    }

    .torrent-row {
        display: grid;
        grid-template-columns: 1fr 1fr 1fr 1.5fr 2fr;
        align-items: center;
        justify-items: center;
        text-align: center;
        padding: var(--spacing-sm) 0;
        border-bottom: 1px solid #222;
        transition: background-color var(--transition-fast);
    }

    .action-buttons {
        display: flex;
        align-items: center;
        gap: 12px;
        justify-content: center;
    }

    .action-buttons .icon-btn {
        background: transparent;
        color: white;
        border: none;
        cursor: pointer;
        padding: 6px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: var(--border-radius-sm);
        transition: all var(--transition-fast);
        opacity: 0.7;
    }

    .action-buttons .icon-btn:hover {
        opacity: 1;
        background-color: rgba(255, 255, 255, 0.1);
        transform: scale(1.1);
    }

    /* SVG Progress Donut */
    .progress-indicator {
        position: relative;
        width: 32px;
        height: 32px;
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .progress-indicator svg {
        transform: rotate(-90deg);
        position: absolute;
        top: 0;
        left: 0;
    }

    .progress-indicator .bar {
        stroke-linecap: round;
        /* Default to green, but we can override if state is checking/seeding */
    }

    .progress-indicator .done-icon {
        position: absolute;
        display: flex;
        align-items: center;
        justify-content: center;
        z-index: 2;
    }

    .torrent-row:last-child {
        border-bottom: none;
    }

    .torrent-row:hover:not(.header) {
        background-color: #1a1a1a;
    }

    .torrent-row.header {
        font-weight: 600;
        color: var(--text-muted);
        text-transform: uppercase;
        font-size: 0.8rem;
        background-color: #0d0d0d;
    }

    .seeders { color: #46d369; margin-right: 10px; font-weight: bold; }
    .leechers { color: var(--accent-color); font-weight: bold; }

    .play-btn {
        background-color: white;
        color: black;
        font-weight: bold;
        padding: 10px 16px;
        border-radius: var(--border-radius-sm);
        transition: background-color var(--transition-fast), transform var(--transition-fast);
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .play-btn:hover {
        background-color: rgba(255,255,255,0.8);
        transform: scale(1.02);
    }

    @media (max-width: 600px) {
        .modal-content { width: 95%; }
        .target-title { font-size: 2rem; }
        .torrent-row { font-size: 0.9rem; grid-template-columns: 80px 100px 1fr;}
        .torrent-row .type, .torrent-row .peers { display: none; } /* Hide type and peers on small screens */
    }
</style>
