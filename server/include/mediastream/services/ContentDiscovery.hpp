#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace media::services {

    // Torrent Quality Information
    struct TorrentQuality {
        std::string quality;      // "720p", "1080p", "2160p"
        std::string type;         // "web", "bluray"
        int64_t size_bytes;
        std::string magnet_uri;
        std::string hash;
        int seeders;
        int leechers;
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
        std::string language;
        std::vector<TorrentQuality> torrents;
        std::string source;  // "YTS", "EZTV", "Nyaa"
    };

    // Base Content Discovery Interface
    class IContentDiscovery {
    public:
        virtual ~IContentDiscovery() = default;
        
        // Fetch popular/trending content
        virtual std::vector<DiscoveredContent> fetchPopular(int limit = 20) = 0;
        
        // Search for specific content
        virtual std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) = 0;
        
        // Get content by ID (if supported)
        virtual std::optional<DiscoveredContent> getById(const std::string& id) = 0;
    };

    // YTS API Client (Movies)
    class YTSClient : public IContentDiscovery {
    public:
        YTSClient();
        ~YTSClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
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

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Nyaa API Client (Anime)
    class NyaaClient : public IContentDiscovery {
    public:
        NyaaClient();
        ~NyaaClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Torrentio API Client (Multi-source Stremio Addon)
    class TorrentioClient : public IContentDiscovery {
    public:
        TorrentioClient();
        ~TorrentioClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
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

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        std::vector<DiscoveredContent> fetchSeries(int limit = 20);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // TMDB Catalog Client
    class TMDBCatalogClient : public IContentDiscovery {
    public:
        TMDBCatalogClient();
        ~TMDBCatalogClient() override;

        std::vector<DiscoveredContent> fetchPopular(int limit = 20) override;
        std::vector<DiscoveredContent> search(const std::string& query, int limit = 20) override;
        std::optional<DiscoveredContent> getById(const std::string& id) override;

        std::vector<DiscoveredContent> fetchSeries(int limit = 20);
        std::vector<DiscoveredContent> fetchAnime(int limit = 20);

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
        std::vector<DiscoveredContent> fetchMovies(int limit = 20);
        std::vector<DiscoveredContent> fetchSeries(int limit = 20);
        std::vector<DiscoveredContent> fetchAnime(int limit = 20);

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
