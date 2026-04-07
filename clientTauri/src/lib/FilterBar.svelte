<script>
    import { createEventDispatcher } from "svelte";
    const dispatch = createEventDispatcher();

    export let selectedGenres = [];
    export let selectedLanguage = "";

    const genres = [
        { id: 28, name: "Action" },
        { id: 12, name: "Adventure" },
        { id: 16, name: "Animation" },
        { id: 35, name: "Comedy" },
        { id: 80, name: "Crime" },
        { id: 99, name: "Documentary" },
        { id: 18, name: "Drama" },
        { id: 10751, name: "Family" },
        { id: 14, name: "Fantasy" },
        { id: 36, name: "History" },
        { id: 27, name: "Horror" },
        { id: 10402, name: "Music" },
        { id: 9648, name: "Mystery" },
        { id: 10749, name: "Romance" },
        { id: 878, name: "Sci-Fi" },
        { id: 53, name: "Thriller" },
        { id: 10752, name: "War" },
        { id: 37, name: "Western" }
    ];

    const languages = [
        { code: "", name: "All Languages" },
        { code: "en", name: "English" },
        { code: "ja", name: "Japanese" },
        { code: "es", name: "Spanish" },
        { code: "fr", name: "French" },
        { code: "de", name: "German" },
        { code: "ko", name: "Korean" },
        { code: "zh", name: "Chinese" }
    ];

    function toggleGenre(id) {
        if (selectedGenres.includes(id)) {
            selectedGenres = selectedGenres.filter(g => g !== id);
        } else {
            selectedGenres = [...selectedGenres, id];
        }
        dispatch("change", { selectedGenres, selectedLanguage });
    }

    function selectLanguage(code) {
        selectedLanguage = code;
        dispatch("change", { selectedGenres, selectedLanguage });
    }
</script>

<div class="filter-bar">
    <div class="filter-group">
        <span class="filter-label">Genres:</span>
        <div class="chips-container">
            {#each genres as genre}
                <button 
                    class="chip" 
                    class:active={selectedGenres.includes(genre.id)}
                    on:click={() => toggleGenre(genre.id)}
                >
                    {genre.name}
                </button>
            {/each}
        </div>
    </div>

    <div class="filter-group">
        <span class="filter-label">Language:</span>
        <div class="chips-container">
            {#each languages as lang}
                <button 
                    class="chip" 
                    class:active={selectedLanguage === lang.code}
                    on:click={() => selectLanguage(lang.code)}
                >
                    {lang.name}
                </button>
            {/each}
        </div>
    </div>
</div>

<style>
    .filter-bar {
        display: flex;
        flex-direction: column;
        gap: 1rem;
        padding: 1rem 2rem;
        background: rgba(255, 255, 255, 0.05);
        backdrop-filter: blur(10px);
        border-radius: 12px;
        margin-bottom: 2rem;
        border: 1px solid rgba(255, 255, 255, 0.1);
    }

    .filter-group {
        display: flex;
        align-items: center;
        gap: 1rem;
    }

    .filter-label {
        font-size: 0.9rem;
        font-weight: 600;
        color: rgba(255, 255, 255, 0.6);
        min-width: 80px;
    }

    .chips-container {
        display: flex;
        flex-wrap: wrap;
        gap: 0.5rem;
    }

    .chip {
        padding: 0.4rem 0.8rem;
        border-radius: 20px;
        background: rgba(255, 255, 255, 0.1);
        border: 1px solid transparent;
        color: white;
        font-size: 0.8rem;
        cursor: pointer;
        transition: all 0.2s ease;
    }

    .chip:hover {
        background: rgba(255, 255, 255, 0.2);
    }

    .chip.active {
        background: var(--accent-color, #0078d4);
        color: white;
        box-shadow: 0 4px 10px rgba(0, 120, 212, 0.3);
    }
</style>
