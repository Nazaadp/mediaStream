#pragma once

#include "mediastream/services/LanguageTags.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace media::services {

    // Torrent Quality Information
    struct TorrentQuality {
        // ── Existing fields ────────────────────────────────────────────────────
        std::string quality;            // "720p", "1080p", "2160p" (or parsed)
        std::string type;               // encode type or legacy source tag
        std::string title;              // display name (1st line for Torrentio)
        std::string source;             // "YTS", "TorrentGalaxy", "EZTV", "NyaaSi", etc.
        // ── Languages (media::services::lang, see LanguageTags.hpp) ──────────
        // The strings hold ONLY what the release name actually asserts —
        // "EN/ES-LA", or "N/A" when it says nothing. "MULTI" is not a value
        // here: it is the `*_multi` flag, so a release can be both multi-audio
        // and enumerated. `*_inferred` marks a code that came from convention
        // (untagged release ⇒ English) rather than from the name, which is
        // what lets the filter tell a real Spanish dub from an unlabelled rip.
        std::string audio_languages;    // "EN", "EN/ES-LA", "N/A"
        std::string subtitle_languages; // "N/A", "EN", "FR"
        bool        audio_multi{false};
        bool        audio_inferred{false};
        bool        subs_multi{false};
        int64_t     size_bytes{0};
        std::string magnet_uri;
        std::string hash;
        int         seeders{0};
        int         leechers{0};

        // ── Multi-file torrent identity (season packs) ────────────────────────
        // A season pack lists the SAME infohash for every episode; only the
        // file inside the torrent differs. file_index carries Torrentio's
        // fileIdx so the engine can stream/prioritize the right file instead
        // of defaulting to the largest one. -1 = unknown → largest-file
        // fallback (single-file torrents, EZTV, Nyaa).
        int         file_index{-1};
        std::string file_name;         // per-file release name (fallback matcher)
        bool        is_pack{false};    // torrent bundles more episodes than the requested one

        // ── Parsed metadata (populated by TorrentScorer::enrich) ──────────────
        int         resolution_p{0};   // 4320, 2160, 1080, 720, 576, 480; 0=unknown
        std::string codec;             // "AV1", "HEVC", "x264", "XviD", ""
        bool        is_hdr{false};     // HDR or HDR10 present
        bool        is_hdr10{false};   // HDR10 specifically
        bool        is_dv{false};      // Dolby Vision
        bool        is_remux{false};   // REMUX / BDRemux
        bool        is_bluray{false};  // BluRay / BDRip source
        bool        is_webdl{false};   // WEB-DL / WEBMux
        bool        is_cam{false};     // CAM / TS / TeleSync — garbage tier
        int         score{0};          // Computed quality score (higher = better)

        // ── Structured parse results (TorrentScorer::parse, via enrich) ───────
        // Additive: brace-init defaults keep every existing consumer compiling
        // and behaving identically. Nothing is serialized yet.
        std::string parsed_title;      // clean title; "" = parse failed, use `title`
        int         year{0};           // 0 = none found
        int         season{0};         // 0 = none
        int         episode{0};        // 0 = none
        int         episode_end{0};    // >0 = multi-episode range (pack)
        std::string release_group;     // "SubsPlease", "SPARKS", "" = unknown
    };

    // ── Torrent name language helpers (used by all clients) ──────────────────
    //
    // Detection lives in LanguageTags.hpp. These three wrappers are the only
    // thing a source client needs: hand them the richest name available and
    // they stamp all four language fields consistently, so no client can
    // half-populate them.

    // Scene / P2P sources (Torrentio, EZTV). For Torrentio pass the FULL title
    // block, not just the filename — the flag emojis and the MULTi tag live on
    // their own lines, and the flags are the only per-track language data any
    // source gives us.
    inline void stampAudioLangs(TorrentQuality& tq, const std::string& name) {
        const lang::Detection d = lang::detectAudio(name);
        tq.audio_languages = d.join();
        tq.audio_multi     = d.multi;
        tq.audio_inferred  = d.inferred;
    }

    // Nyaa: Japanese audio by default, [Dual-Audio] ⇒ JA+EN.
    inline void stampAnimeAudioLangs(TorrentQuality& tq, const std::string& name) {
        const lang::Detection d = lang::detectAnimeAudio(name);
        tq.audio_languages = d.join();
        tq.audio_multi     = d.multi;
        tq.audio_inferred  = d.inferred;
    }

    inline void stampSubtitleLangs(TorrentQuality& tq, const std::string& name) {
        const lang::Detection d = lang::detectSubs(name);
        tq.subtitle_languages = d.join();
        tq.subs_multi         = d.multi;
    }

    // Map an ISO 639-1 language code (lowercase, e.g. "en", "fr") to uppercase display.
    inline std::string isoToDisplayLang(const std::string& code) {
        if (code.empty()) return "EN";
        std::string up = code;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        return up;
    }

    // ── Torrent result filters (?res / ?audio / ?subs query params) ──────────

    struct TorrentFilterCriteria {
        std::unordered_set<std::string> resolutions; // "2160", "1080", ...
        // Uppercase language codes, plus the "MULTI" pseudo-code meaning
        // "any release with multiple tracks, whichever they are".
        std::unordered_set<std::string> audio;
        std::unordered_set<std::string> subs;
        bool empty() const { return resolutions.empty() && audio.empty() && subs.empty(); }
    };

    // Applies the criteria in place. Must run on the FULL aggregated list,
    // before score-sort truncation — otherwise 4K releases (which score
    // highest) crowd every lower-resolution pick out of the capped result.
    //
    // Every dimension is strict: a torrent passes only on evidence. Resolution
    // drops unknown/0; language drops anything the release name never claimed,
    // including multi-audio releases unless MULTI is one of the requested
    // codes. The old rule let MULTI / DUAL / N/A satisfy every language filter,
    // which inverted the feature — asking for Spanish returned nothing BUT
    // multi-audio releases, and those mostly carry other languages entirely.
    // See lang::matchesFilter for the full rationale.
    inline void applyTorrentFilters(std::vector<TorrentQuality>& torrents,
                                    const TorrentFilterCriteria& f) {
        if (f.empty()) return;
        std::erase_if(torrents, [&](const TorrentQuality& t) {
            if (!f.resolutions.empty() &&
                !f.resolutions.count(std::to_string(t.resolution_p))) return true;
            if (!lang::matchesFilter(t.audio_languages, t.audio_multi, f.audio))
                return true;
            if (!lang::matchesFilter(t.subtitle_languages, t.subs_multi, f.subs))
                return true;
            return false;
        });
    }

    // Season summary (from TMDB)
    struct SeasonInfo {
        int season_number;
        std::string name;
        int episode_count;
        std::string poster_url;
        float rating = 0.0f;
        std::string air_date;   // TMDB "YYYY-MM-DD"; empty when unknown
    };

    // Episode summary (from TMDB)
    struct EpisodeInfo {
        int episode_number;
        int season_number;
        std::string name;
        std::string overview;
        std::string still_url;
        float rating;
        std::string air_date;   // TMDB "YYYY-MM-DD"; empty when unknown
    };

    // Discovered Content Item
    struct DiscoveredContent {
        std::string title;
        std::string original_title;
        int year;
        std::string description;
        std::string poster_url;
        std::string backdrop_url;
        float rating;
        std::string genres;
        int runtime_minutes;
        std::string imdb_id;
        std::string tmdb_id;
        std::string language;
        std::string original_language;
        std::vector<TorrentQuality> torrents;
        std::string source;  // "YTS", "EZTV", "Nyaa"
        std::string type;    // "movie" or "tv"
    };

    // Base Content Discovery Interface
    class IContentDiscovery {
    public:
        virtual ~IContentDiscovery() = default;
        
        // Fetch popular/trending content
        virtual std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") = 0;
        
        // Search for specific content
        virtual std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") = 0;
        
        // Get content by ID (if supported)
        virtual std::optional<DiscoveredContent> getById(const std::string& id) = 0;
    };

    // YTS API Client (Movies)
    class YTSClient : public IContentDiscovery {
    public:
        YTSClient();
        ~YTSClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // EZTV API Client (TV Series)
    class EZTVClient : public IContentDiscovery {
    public:
        EZTVClient();
        ~EZTVClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        // On-demand: torrents for a specific episode identified by IMDB ID
        std::vector<TorrentQuality> fetchEpisodeTorrents(const std::string& imdb_id, int season, int episode, const std::string& title = "");

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Nyaa API Client (Anime)
    class NyaaClient : public IContentDiscovery {
    public:
        NyaaClient();
        ~NyaaClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        // On-demand: torrents for a specific episode by title + episode number
        std::vector<TorrentQuality> fetchEpisodeTorrents(const std::string& title, int episode);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Torrentio API Client (Multi-source Stremio Addon)
    class TorrentioClient : public IContentDiscovery {
    public:
        TorrentioClient();
        ~TorrentioClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        // Specific Torrentio Fetch for precise matching
        std::vector<DiscoveredContent> searchTorrentsByIMDB(const std::string& imdb_id, const std::string& type, int season = -1, int episode = -1);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Stremio Cinemeta Catalog Client
    class CinemetaClient : public IContentDiscovery {
    public:
        CinemetaClient();
        ~CinemetaClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        std::vector<DiscoveredContent> fetchSeries(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // TMDB Catalog Client
    class TMDBCatalogClient : public IContentDiscovery {
    public:
        TMDBCatalogClient();
        ~TMDBCatalogClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "") override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        std::vector<DiscoveredContent> fetchSeries(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");
        std::vector<DiscoveredContent> fetchAnime(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    class TMDBFetcher;

    // Unified Content Discovery Manager
    class ContentDiscoveryManager {
    public:
        ContentDiscoveryManager();
        ~ContentDiscoveryManager();

        // Fetch from all sources
        std::vector<DiscoveredContent> fetchAllPopular(int limit_per_source = 20);
        
        // Search across all sources
        std::vector<DiscoveredContent> searchAll(const std::string& query, int limit_per_source = 20);
        
        // Fetch by type
        std::vector<DiscoveredContent> fetchMovies(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");
        std::vector<DiscoveredContent> fetchSeries(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");
        std::vector<DiscoveredContent> fetchAnime(int limit = 20, int page = 1, const std::string& genre = "", const std::string& language = "");

        // On-demand movie torrents (called when user opens a movie card).
        // filters are applied to the full aggregated list before truncation.
        std::vector<TorrentQuality> fetchMovieTorrents(const std::string& imdb_id,
                                                       const TorrentFilterCriteria& filters = {});

        // On-demand episode navigation (called when user opens a series card)
        std::vector<SeasonInfo>  fetchSeasons(const std::string& imdb_id);
        std::vector<EpisodeInfo> fetchEpisodes(const std::string& imdb_id, int season);
        // title is forwarded to Nyaa for anime episodes.
        // filters are applied to the full aggregated list before truncation.
        std::vector<TorrentQuality> fetchEpisodeTorrents(const std::string& imdb_id, int season, int episode,
                                                         const std::string& title = "",
                                                         const TorrentFilterCriteria& filters = {});

        // On-demand genres (called when user opens a media card)
        std::string fetchGenres(const std::string& imdb_id);

    private:
        std::unique_ptr<YTSClient> m_yts;
        std::unique_ptr<EZTVClient> m_eztv;
        std::unique_ptr<NyaaClient> m_nyaa;
        std::unique_ptr<TorrentioClient> m_torrentio;
        std::unique_ptr<TMDBFetcher> m_tmdb;
        std::unique_ptr<CinemetaClient> m_cinemeta;
        std::unique_ptr<TMDBCatalogClient> m_tmdb_catalog;
    };

} // namespace media::services

// Made with Bob
