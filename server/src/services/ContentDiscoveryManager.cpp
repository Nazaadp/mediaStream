#include "mediastream/services/ContentDiscovery.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace media::services {

    ContentDiscoveryManager::ContentDiscoveryManager() 
        : m_yts(std::make_unique<YTSClient>())
        , m_eztv(std::make_unique<EZTVClient>())
        , m_nyaa(std::make_unique<NyaaClient>()) {
        spdlog::info("Content Discovery Manager initialized");
    }

    ContentDiscoveryManager::~ContentDiscoveryManager() = default;

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchAllPopular(int limit_per_source) {
        std::vector<DiscoveredContent> all_content;

        // Fetch from all sources in parallel (simplified sequential for now)
        try {
            auto movies = m_yts->fetchPopular(limit_per_source);
            all_content.insert(all_content.end(), movies.begin(), movies.end());
            spdlog::info("Fetched {} movies from YTS", movies.size());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch from YTS: {}", e.what());
        }

        try {
            auto series = m_eztv->fetchPopular(limit_per_source);
            all_content.insert(all_content.end(), series.begin(), series.end());
            spdlog::info("Fetched {} series from EZTV", series.size());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch from EZTV: {}", e.what());
        }

        try {
            auto anime = m_nyaa->fetchPopular(limit_per_source);
            all_content.insert(all_content.end(), anime.begin(), anime.end());
            spdlog::info("Fetched {} anime from Nyaa", anime.size());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch from Nyaa: {}", e.what());
        }

        // Sort by rating/seeders
        std::sort(all_content.begin(), all_content.end(), 
            [](const DiscoveredContent& a, const DiscoveredContent& b) {
                // Sort by number of seeders in best torrent
                int a_seeders = 0, b_seeders = 0;
                if (!a.torrents.empty()) {
                    a_seeders = std::max_element(a.torrents.begin(), a.torrents.end(),
                        [](const TorrentQuality& x, const TorrentQuality& y) {
                            return x.seeders < y.seeders;
                        })->seeders;
                }
                if (!b.torrents.empty()) {
                    b_seeders = std::max_element(b.torrents.begin(), b.torrents.end(),
                        [](const TorrentQuality& x, const TorrentQuality& y) {
                            return x.seeders < y.seeders;
                        })->seeders;
                }
                return a_seeders > b_seeders;
            });

        spdlog::info("Total content discovered: {}", all_content.size());
        return all_content;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::searchAll(const std::string& query, int limit_per_source) {
        std::vector<DiscoveredContent> all_results;

        try {
            auto movies = m_yts->search(query, limit_per_source);
            all_results.insert(all_results.end(), movies.begin(), movies.end());
        } catch (const std::exception& e) {
            spdlog::error("YTS search failed: {}", e.what());
        }

        try {
            auto series = m_eztv->search(query, limit_per_source);
            all_results.insert(all_results.end(), series.begin(), series.end());
        } catch (const std::exception& e) {
            spdlog::error("EZTV search failed: {}", e.what());
        }

        try {
            auto anime = m_nyaa->search(query, limit_per_source);
            all_results.insert(all_results.end(), anime.begin(), anime.end());
        } catch (const std::exception& e) {
            spdlog::error("Nyaa search failed: {}", e.what());
        }

        spdlog::info("Search '{}' returned {} results", query, all_results.size());
        return all_results;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchMovies(int limit) {
        try {
            return m_yts->fetchPopular(limit);
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch movies: {}", e.what());
            return {};
        }
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchSeries(int limit) {
        try {
            return m_eztv->fetchPopular(limit);
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch series: {}", e.what());
            return {};
        }
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchAnime(int limit) {
        try {
            return m_nyaa->fetchPopular(limit);
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch anime: {}", e.what());
            return {};
        }
    }

} // namespace media::services

// Made with Bob
