#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/services/TMDBFetcher.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <future>
#include <set>

namespace media::services {

    ContentDiscoveryManager::ContentDiscoveryManager() 
        : m_yts(std::make_unique<YTSClient>())
        , m_eztv(std::make_unique<EZTVClient>())
        , m_nyaa(std::make_unique<NyaaClient>())
        , m_torrentio(std::make_unique<TorrentioClient>())
        , m_tmdb(std::make_unique<TMDBFetcher>())
        , m_cinemeta(std::make_unique<CinemetaClient>())
        , m_tmdb_catalog(std::make_unique<TMDBCatalogClient>()) {
        spdlog::info("Content Discovery Manager initialized");
    }
    namespace {
        void enrichAndDeduplicate(std::vector<DiscoveredContent>& list, TMDBFetcher* tmdb, TorrentioClient* tio) {
            std::vector<std::future<void>> futures;
            spdlog::info("Asynchronously enriching {} items...", list.size());

            for (auto& item : list) {
                // Launch asynchronous task for EACH item to drastically reduce latency
                futures.push_back(std::async(std::launch::async, [&item, tmdb, tio]() {
                    tmdb->enrichContent(item);

                    if (!item.imdb_id.empty()) {
                        auto type = item.type.empty() ? (item.source == "EZTV" || item.source == "Nyaa" ? "tv" : "movie") : item.type;
                        auto tio_res = tio->searchTorrentsByIMDB(item.imdb_id, type);
                        if (!tio_res.empty() && !tio_res[0].torrents.empty()) {
                            item.torrents.insert(item.torrents.end(), tio_res[0].torrents.begin(), tio_res[0].torrents.end());
                        }
                    }

                    std::unordered_map<std::string, TorrentQuality> deduped;
                    for (const auto& t : item.torrents) {
                        std::string hash_lower = t.hash;
                        std::transform(hash_lower.begin(), hash_lower.end(), hash_lower.begin(), ::tolower);
                        if (deduped.find(hash_lower) == deduped.end() || t.seeders > deduped[hash_lower].seeders) {
                            deduped[hash_lower] = t;
                        }
                    }
                    item.torrents.clear();
                    for (const auto& [hash, tq] : deduped) {
                        item.torrents.push_back(tq);
                    }
                    std::sort(item.torrents.begin(), item.torrents.end(), [](const TorrentQuality& a, const TorrentQuality& b){
                        return a.seeders > b.seeders;
                    });
                }));
            }

            // Await all parallel enrichment threads
            for (auto& f : futures) {
                f.wait(); 
            }
        }
    }

    ContentDiscoveryManager::~ContentDiscoveryManager() = default;

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchAllPopular(int limit_per_source) {
        std::vector<DiscoveredContent> all_content;

        // Fetch from all sources in parallel
        try {
            auto f_movies = std::async(std::launch::async, [&]{ return m_yts->fetchPopular(limit_per_source); });
            auto f_series = std::async(std::launch::async, [&]{ return m_eztv->fetchPopular(limit_per_source); });
            auto f_anime  = std::async(std::launch::async, [&]{ return m_nyaa->fetchPopular(limit_per_source); });
            
            try {
                auto movies = f_movies.get();
                all_content.insert(all_content.end(), movies.begin(), movies.end());
                spdlog::info("Fetched {} movies from YTS", movies.size());
            } catch (const std::exception& e) {
                spdlog::error("Failed to fetch from YTS: {}", e.what());
            }

            try {
                auto series = f_series.get();
                all_content.insert(all_content.end(), series.begin(), series.end());
                spdlog::info("Fetched {} series from EZTV", series.size());
            } catch (const std::exception& e) {
                spdlog::error("Failed to fetch from EZTV: {}", e.what());
            }

            try {
                auto anime = f_anime.get();
                all_content.insert(all_content.end(), anime.begin(), anime.end());
                spdlog::info("Fetched {} anime from Nyaa", anime.size());
            } catch (const std::exception& e) {
                spdlog::error("Failed to fetch from Nyaa: {}", e.what());
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to launch async fetches: {}", e.what());
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
        
        enrichAndDeduplicate(all_content, m_tmdb.get(), m_torrentio.get());
        
        return all_content;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::searchAll(const std::string& query, int limit_per_source) {
        std::vector<DiscoveredContent> all_results;

        try {
            auto tmdb = m_tmdb_catalog->search(query, limit_per_source, 1);
            all_results.insert(all_results.end(), tmdb.begin(), tmdb.end());
        } catch (const std::exception& e) {
            spdlog::error("TMDB search failed: {}", e.what());
        }

        try {
            auto cine = m_cinemeta->search(query, limit_per_source, 1);
            all_results.insert(all_results.end(), cine.begin(), cine.end());
        } catch (const std::exception& e) {
            spdlog::error("Cinemeta search failed: {}", e.what());
        }

        spdlog::info("Search '{}' returned {} raw results", query, all_results.size());

        // Deduplicate results based on IDs (TMDB or IMDB)
        std::vector<DiscoveredContent> unique_results;
        std::set<std::string> seen_ids;
        for (const auto& item : all_results) {
            std::string id = item.tmdb_id.empty() ? item.imdb_id : item.tmdb_id;
            if (!id.empty()) {
                if (seen_ids.find(id) != seen_ids.end()) continue;
                seen_ids.insert(id);
            }
            unique_results.push_back(item);
        }
        all_results = std::move(unique_results);

        // Sort results: Primary by Year (Desc), Secondary by Rating (Desc)
        std::sort(all_results.begin(), all_results.end(), [](const DiscoveredContent& a, const DiscoveredContent& b) {
            if (a.year != b.year) {
                return a.year > b.year; // More recent first
            }
            return b.rating > a.rating; // Higher rated first (Wait, b.rating > a.rating is Ascending? No, a.rating > b.rating is Descending. Wait.)
        });
        
        // Correct sorting for Descending: a > b
        std::sort(all_results.begin(), all_results.end(), [](const DiscoveredContent& a, const DiscoveredContent& b) {
            if (a.year != b.year) return a.year > b.year;
            return a.rating > b.rating;
        });

        // Cap the total results before enrichment to prevent 100+ Torrentio requests
        // using our 500ms rate limiter (which would take ~50s).
        if (all_results.size() > 20) {
            all_results.resize(20);
            spdlog::info("Capped search results to 20 for enrichment stability.");
        }
        
        enrichAndDeduplicate(all_results, m_tmdb.get(), m_torrentio.get());

        return all_results;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchMovies(int limit, int page) {
        std::vector<DiscoveredContent> results;
        try {
            auto cine = m_cinemeta->fetchPopular(limit, page);
            auto tmdb = m_tmdb_catalog->fetchPopular(limit, page);

            results.insert(results.end(), tmdb.begin(), tmdb.end());
            results.insert(results.end(), cine.begin(), cine.end());

            // Cap the concatenated catalog to the requested limit so we don't enrich 60+ items per row.
            // This massively reduces the parallel requests sent to Torrentio, obeying CF limits.
            if (limit > 0 && results.size() > static_cast<size_t>(limit)) {
                results.resize(limit);
            }

            enrichAndDeduplicate(results, m_tmdb.get(), m_torrentio.get());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch movies: {}", e.what());
        }
        return results;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchSeries(int limit, int page) {
        std::vector<DiscoveredContent> results;
        try {
            auto cine = m_cinemeta->fetchSeries(limit, page);
            auto tmdb = m_tmdb_catalog->fetchSeries(limit, page);

            results.insert(results.end(), tmdb.begin(), tmdb.end());
            results.insert(results.end(), cine.begin(), cine.end());

            if (limit > 0 && results.size() > static_cast<size_t>(limit)) {
                results.resize(limit);
            }

            enrichAndDeduplicate(results, m_tmdb.get(), m_torrentio.get());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch series: {}", e.what());
        }
        return results;
    }

    std::vector<DiscoveredContent> ContentDiscoveryManager::fetchAnime(int limit, int page) {
        std::vector<DiscoveredContent> results;
        try {
            // Cinemeta doesn't have an Anime-specific root catalog by default, so we fall back to TMDB natively
            auto tmdb = m_tmdb_catalog->fetchAnime(limit, page);

            results.insert(results.end(), tmdb.begin(), tmdb.end());

            if (limit > 0 && results.size() > static_cast<size_t>(limit)) {
                results.resize(limit);
            }

            enrichAndDeduplicate(results, m_tmdb.get(), m_torrentio.get());
        } catch (const std::exception& e) {
            spdlog::error("Failed to fetch anime: {}", e.what());
        }
        return results;
    }

} // namespace media::services

// Made with Bob
