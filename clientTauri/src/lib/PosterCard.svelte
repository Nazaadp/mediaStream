<script>
    export let item;
    export let focused = false;

    import { createEventDispatcher, afterUpdate } from 'svelte';
    const dispatch = createEventDispatcher();

    let element;

    afterUpdate(() => {
        if (focused && element) {
            element.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' });
        }
    });

    function handleClick() {
        dispatch('select', item);
    }
</script>

<!-- svelte-ignore a11y-click-events-have-key-events a11y-no-static-element-interactions -->
<div 
    bind:this={element}
    class="poster-card" 
    class:focused={focused} 
    on:click={handleClick}
>
    <div class="image-wrapper">
        <img src={item.poster_url || 'https://via.placeholder.com/300x450?text=No+Poster'} alt={item.title || item.original_title} loading="lazy" />
    </div>
    <div class="title-wrapper">
        <span class="title" title={item.title || item.original_title}>{item.title || item.original_title}</span>
    </div>
</div>

<style>
    .poster-card {
        flex: 0 0 auto;
        width: 140px; /* Base size */
        margin-right: var(--spacing-md);
        cursor: pointer;
        transition: transform var(--transition-normal), border var(--transition-normal), box-shadow var(--transition-normal);
        border: 2px solid transparent;
        border-radius: var(--border-radius-md);
        overflow: hidden;
        background-color: #222;
        display: flex;
        flex-direction: column;
    }

    .poster-card:hover, .poster-card.focused {
        transform: scale(1.05) translateY(-5px); /* Graceful upscale and slight lift */
        border: 2px solid var(--accent-color, #0078d4);
        box-shadow: 0 10px 20px rgba(0,0,0,0.8);
        z-index: 10;
    }

    .image-wrapper {
        width: 100%;
        aspect-ratio: 2 / 3;
        background-color: #111;
        overflow: hidden;
    }

    .image-wrapper img {
        width: 100%;
        height: 100%;
        object-fit: cover;
    }

    .title-wrapper {
        padding: var(--spacing-sm);
        text-align: center;
        flex-grow: 1;
        display: flex;
        align-items: center;
        justify-content: center;
        background: linear-gradient(0deg, #181818 0%, #222 100%);
    }

    .title {
        font-size: 0.85rem;
        font-weight: 500;
        color: var(--text-muted);
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
        max-width: 100%;
        transition: color var(--transition-fast);
    }

    .poster-card:hover .title {
        color: var(--text-main);
    }

    @media (min-width: 768px) {
        .poster-card {
            width: 180px;
        }
    }
    
    @media (min-width: 1400px) {
        .poster-card {
            width: 220px;
        }
    }
</style>
