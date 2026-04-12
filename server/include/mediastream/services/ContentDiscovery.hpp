#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

namespace media::services {

    // Torrent Quality Information
    struct TorrentQuality {
        std::string quality;            // "720p", "1080p", "2160p"
        std::string type;               // encode type or legacy source tag
        std::string title;              // full torrent name / filename
        std::string source;             // "YTS", "TorrentGalaxy", "EZTV", "Nyaa", etc.
        std::string audio_languages;    // "EN", "FR", "EN/FR", "MULTI", "N/A"
        std::string subtitle_languages; // "N/A", "EN", "FR", "MULTI"
        int64_t size_bytes{0};
        std::string magnet_uri;
        std::string hash;
        int seeders{0};
        int leechers{0};
    };

    // ── Torrent name language helpers (used by all clients) ──────────────────

    inline std::string parseTorrentAudioLangs(const std::string& name) {
        std::string n = name;
        std::transform(n.begin(), n.end(), n.begin(), ::toupper);
        for (auto& c : n) if (c == '.' || c == '_' || c == '-') c = ' ';
        n = " " + n + " ";

        bool vostfr = (n.find("VOSTFR") != std::string::npos);
        std::string result = vostfr ? "" : "EN";

        // Collect language tags in order
        if (n.find(" FRENCH ") != std::string::npos || n.find(" TRUEFRENCH ") != std::string::npos ||
            n.find(" VF ") != std::string::npos || n.find(" VFHQ ") != std::string::npos) {
            if (result.find("FR") == std::string::npos) result += (result.empty() ? "FR" : "/FR");
        }
        if (n.find(" SPANISH ") != std::string::npos || n.find(" ESP ") != std::string::npos) {
            if (result.find("ES") == std::string::npos) result += (result.empty() ? "ES" : "/ES");
        }
        if (n.find(" GERMAN ") != std::string::npos || n.find(" GER ") != std::string::npos) {
            if (result.find("DE") == std::string::npos) result += (result.empty() ? "DE" : "/DE");
        }
        if (n.find(" PORTUGUESE ") != std::string::npos || n.find(" POR ") != std::string::npos) {
            if (result.find("PT") == std::string::npos) result += (result.empty() ? "PT" : "/PT");
        }
        if (n.find(" ITALIAN ") != std::string::npos || n.find(" ITA ") != std::string::npos) {
            if (result.find("IT") == std::string::npos) result += (result.empty() ? "IT" : "/IT");
        }
        if (n.find(" JAPANESE ") != std::string::npos || n.find(" JPN ") != std::string::npos) {
            if (result.find("JA") == std::string::npos) result += (result.empty() ? "JA" : "/JA");
        }
        if (n.find(" KOREAN ") != std::string::npos || n.find(" KOR ") != std::string::npos) {
            if (result.find("KO") == std::string::npos) result += (result.empty() ? "KO" : "/KO");
        }
        if (n.find(" RUSSIAN ") != std::string::npos || n.find(" RUS ") != std::string::npos) {
            if (result.find("RU") == std::string::npos) result += (result.empty() ? "RU" : "/RU");
        }
        if (n.find(" HINDI ") != std::string::npos) {
            if (result.find("HI") == std::string::npos) result += (result.empty() ? "HI" : "/HI");
        }
        if (n.find(" ARABIC ") != std::string::npos) {
            if (result.find("AR") == std::string::npos) result += (result.empty() ? "AR" : "/AR");
        }
        if (n.find(" TURKISH ") != std::string::npos) {
            if (result.find("TR") == std::string::npos) result += (result.empty() ? "TR" : "/TR");
        }

        // If no specific languages found, check for MULTI/DUAL tags
        if (result.empty() || result == "EN") {
            if (n.find(" MULTI ") != std::string::npos || n.find(" MULTI AUDIO") != std::string::npos) {
                return "MULTI";
            }
            if (n.find(" DUAL ") != std::string::npos || n.find(" DUAL AUDIO") != std::string::npos) {
                return "DUAL";
            }
        }

        return result.empty() ? "EN" : result;
    }

    inline std::string parseTorrentSubtitleLangs(const std::string& name) {
        std::string n = name;
        std::transform(n.begin(), n.end(), n.begin(), ::toupper);
        for (auto& c : n) if (c == '.' || c == '_') c = ' ';
        n = " " + n + " ";

        if (n.find(" MULTI SUB") != std::string::npos) return "MULTI";

        // Streaming sources typically include multi-language subtitles
        if (n.find(" NF ") != std::string::npos  || n.find("-NF ")   != std::string::npos ||
            n.find(" AMZN ") != std::string::npos || n.find("-AMZN ") != std::string::npos ||
            n.find(" DSNP ") != std::string::npos || n.find(" HMAX ") != std::string::npos ||
            n.find(" ATVP ") != std::string::npos || n.find(" PCOK ") != std::string::npos) {
            return "MULTI";
        }
        if (n.find("VOSTFR") != std::string::npos ||
            n.find(" SUBFRENCH") != std::string::npos) return "FR";
        if (n.find(" SUBBED") != std::string::npos ||
            n.find(" ENGSUB") != std::string::npos ||
            n.find(" ENG SUB") != std::string::npos) return "EN";

        return "N/A";
    }

    // Map an ISO 639-1 language code (lowercase, e.g. "en", "fr") to uppercase display.
    inline std::string isoToDisplayLang(const std::string& code) {
        if (code.empty()) return "EN";
        std::string up = code;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        return up;
    }

    // Season summary (from TMDB)
    struct SeasonInfo {
        int season_number;
        std::string name;
        int episode_count;
        std::string poster_url;
    };

    // Episode summary (from TMDB)
    struct EpisodeInfo {
        int episode_number;
        int season_number;
        std::string name;
        std::string overview;
        std::string still_url;
        float rating;
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

        // On-demand movie torrents (called when user opens a movie card)
        std::vector<TorrentQuality> fetchMovieTorrents(const std::string& imdb_id);

        // On-demand episode navigation (called when user opens a series card)
        std::vector<SeasonInfo>  fetchSeasons(const std::string& imdb_id);
        std::vector<EpisodeInfo> fetchEpisodes(const std::string& imdb_id, int season);
        // title is forwarded to Nyaa for anime episodes
        std::vector<TorrentQuality> fetchEpisodeTorrents(const std::string& imdb_id, int season, int episode, const std::string& title = "");

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
