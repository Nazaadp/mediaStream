#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/services/TorrentScorer.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <regex>

using json = nlohmann::json;

namespace media::services {

    // Helper: CURL write callback
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    // Helper: Perform HTTP GET request
    static std::string httpGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MediaStream/2.0");

            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                spdlog::error("CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_easy_cleanup(curl);
        }

        return response;
    }

    // EZTV Client Implementation
    class EZTVClient::Impl {
    public:
        const std::string BASE_URL = "https://eztvx.to/api";

        std::vector<DiscoveredContent> parseShowList(const std::string& json_response) {
            std::vector<DiscoveredContent> results;

            try {
                auto j = json::parse(json_response);
                
                if (!j.contains("torrents")) {
                    spdlog::warn("EZTV API response missing torrents");
                    return results;
                }

                auto torrents = j["torrents"];
                
                // Group by show
                std::map<std::string, DiscoveredContent> shows;

                for (const auto& torrent : torrents) {
                    std::string title = torrent.value("title", "");
                    if (title.empty()) continue;

                    // Extract show name (before season/episode info)
                    std::string show_name = title.substr(0, title.find(" S"));
                    
                    if (shows.find(show_name) == shows.end()) {
                        DiscoveredContent content;
                        content.title = show_name;
                        content.original_title = show_name;
                        content.description = "TV Series";
                        content.source = "EZTV";
                        content.language = "en";
                        content.genres = "TV Series";
                        shows[show_name] = content;
                    }

                    // Add torrent
                    TorrentQuality tq;
                    tq.quality = "720p"; // EZTV typically 720p
                    tq.type = "EZTV";
                    if (torrent["size_bytes"].is_string()) {
                        tq.size_bytes = std::stoull(torrent["size_bytes"].get<std::string>());
                    } else if (torrent["size_bytes"].is_number()) {
                        tq.size_bytes = torrent["size_bytes"].get<uint64_t>();
                    } else {
                        tq.size_bytes = 0;
                    }
                    tq.hash = torrent.value("hash", "");
                    tq.seeders = torrent.value("seeds", 0);
                    tq.leechers = torrent.value("peers", 0);
                    tq.magnet_uri = torrent.value("magnet_url", "");

                    shows[show_name].torrents.push_back(tq);
                }

                // Convert map to vector
                for (auto& [name, content] : shows) {
                    results.push_back(content);
                }

            } catch (const json::exception& e) {
                spdlog::error("EZTV JSON parsing error: {}", e.what());
            }

            return results;
        }
    };

    // Constructor/Destructor
    EZTVClient::EZTVClient() : m_impl(std::make_unique<Impl>()) {
        spdlog::info("EZTV Client initialized");
    }

    EZTVClient::~EZTVClient() = default;

    // Fetch Popular Series
    std::vector<DiscoveredContent> EZTVClient::fetchPopular(int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        std::string url = m_impl->BASE_URL + "/get-torrents?limit=" + std::to_string(limit);
        
        spdlog::info("Fetching popular series from EZTV...");
        std::string response = httpGet(url);
        
        if (response.empty()) {
            spdlog::error("Failed to fetch from EZTV");
            return {};
        }

        auto results = m_impl->parseShowList(response);
        
        // Limit results
        if (results.size() > static_cast<size_t>(limit)) {
            results.resize(limit);
        }

        return results;
    }

    // Search Series
    std::vector<DiscoveredContent> EZTVClient::search(const std::string& query, int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        // EZTV search is limited, we'll filter results
        spdlog::info("Searching EZTV for: {}", query);
        
        auto all_results = fetchPopular(100);
        std::vector<DiscoveredContent> filtered;

        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

        for (const auto& content : all_results) {
            std::string lower_title = content.title;
            std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
            
            if (lower_title.find(lower_query) != std::string::npos) {
                filtered.push_back(content);
                if (filtered.size() >= static_cast<size_t>(limit)) break;
            }
        }

        return filtered;
    }

    // Get Series by ID
    std::optional<DiscoveredContent> EZTVClient::getById([[maybe_unused]] const std::string& id) {
        spdlog::warn("EZTV getById not fully implemented");
        return std::nullopt;
    }

    // Fetch torrents for a specific IMDB ID + season + episode.
    // EZTV supports ?imdb_id={numeric} — we filter by S/E from the title client-side.
    std::vector<TorrentQuality> EZTVClient::fetchEpisodeTorrents(
        const std::string& imdb_id, int season, int episode, [[maybe_unused]] const std::string& title)
    {
        std::vector<TorrentQuality> results;
        if ( imdb_id.empty() || imdb_id.size() < 4) return results;

        // Strip "tt" prefix and leading zeros for EZTV
        std::string numeric_id = imdb_id;
        if (numeric_id.substr(0, 2) == "tt") numeric_id = numeric_id.substr(2);
        numeric_id = std::to_string(std::stoi(numeric_id)); // remove leading zeros

        std::string url = m_impl->BASE_URL + "/get-torrents?imdb_id=" + numeric_id + "&limit=100";
        spdlog::info("EZTV episode fetch: {}", url);
        std::string response = httpGet(url);
        if (response.empty()) return results;

        try {
            auto j = json::parse(response);
            if (!j.contains("torrents")) return results;

            // Build regex for S{season}E{episode} in the torrent title
            char se_buf[32];
            std::snprintf(se_buf, sizeof(se_buf), "[Ss]%02d[Ee]%02d", season, episode);
            std::regex se_re(se_buf);

            for (const auto& t : j["torrents"]) {
                std::string title = t.value("title", "");
                if (!std::regex_search(title, se_re)) continue;

                TorrentQuality tq;
                tq.title   = title;
                tq.quality = title; // keep quality in sync for legacy consumers
                tq.type    = "EZTV";
                tq.source  = "EZTV";
                tq.audio_languages    = parseTorrentAudioLangs(title);
                tq.subtitle_languages = parseTorrentSubtitleLangs(title);
                if (t["size_bytes"].is_string()) {
                    tq.size_bytes = std::stoull(t["size_bytes"].get<std::string>());
                } else if (t["size_bytes"].is_number()) {
                    tq.size_bytes = t["size_bytes"].get<int64_t>();
                }
                tq.hash = t.value("hash", "");
                tq.seeders = t.value("seeds", 0);
                tq.leechers = t.value("peers", 0);
                tq.magnet_uri = t.value("magnet_url", "");

                // Parse resolution/codec/HDR/release from the release name and
                // rewrite the legacy quality string (was hard-coded "720p").
                TorrentScorer::enrich(tq, tq.title);

                results.push_back(tq);
            }
            spdlog::info("EZTV: {} torrents for {} S{:02d}E{:02d}", results.size(), imdb_id, season, episode);
        } catch (const std::exception& e) {
            spdlog::error("EZTV fetchEpisodeTorrents parse error: {}", e.what());
        }
        return results;
    }

} // namespace media::services

// Made with Bob
