#include "mediastream/services/ContentDiscovery.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>
#include <sstream>

using json = nlohmann::json;

namespace media::services {

    static size_t TWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static std::string httpGetTimeout(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L); // 8 sec timeout identical to Rattin
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MediaStream/2.0");

            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                spdlog::warn("Torrentio CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_easy_cleanup(curl);
        }

        return response;
    }

    class TorrentioClient::Impl {
    public:
        const std::string BASE_URL = "https://torrentio.strem.fun";

        int64_t parseSizeStr(const std::string& title) {
            const std::string units[] = {"GB", "MB", "KB"};
            for (const auto& unit : units) {
                auto unit_pos = title.find(" " + unit);
                if (unit_pos != std::string::npos && unit_pos > 0) {
                    size_t start = unit_pos - 1;
                    while (start > 0 && (isdigit(title[start]) || title[start] == '.')) {
                        start--;
                    }
                    if (!isdigit(title[start])) start++;
                    try {
                        double num = std::stod(title.substr(start, unit_pos - start));
                        if (unit == "GB") return static_cast<int64_t>(num * 1024 * 1024 * 1024);
                        if (unit == "MB") return static_cast<int64_t>(num * 1024 * 1024);
                        if (unit == "KB") return static_cast<int64_t>(num * 1024);
                    } catch(...) {}
                }
            }
            return 0;
        }

        int parseSeeders(const std::string& title) {
            // The emoji 👤 is represented as the UTF-8 bytes \xF0\x9F\x91\xA4.
            auto pos = title.find("👤");
            if (pos != std::string::npos) {
                size_t start = pos + 4; // Emoji length
                while (start < title.length() && isspace((unsigned char)title[start])) start++;
                
                size_t end = start;
                while (end < title.length() && isdigit((unsigned char)title[end])) end++;
                
                if (start < end) {
                    try {
                        return std::stoi(title.substr(start, end - start));
                    } catch(...) {}
                }
            }
            return 0;
        }

        std::string parseName(const std::string& title) {
            std::istringstream stream(title);
            std::string first_line;
            if (std::getline(stream, first_line)) {
                return first_line;
            }
            return "Unknown";
        }

        std::vector<DiscoveredContent> parseStreams(const std::string& json_response) {
            std::vector<DiscoveredContent> results;
            if (json_response.empty()) return results;

            try {
                auto j = json::parse(json_response);
                
                if (!j.contains("streams") || !j["streams"].is_array()) {
                    return results;
                }

                auto streams = j["streams"];
                DiscoveredContent unifiedContent; // Grouping streams under one virtual item
                unifiedContent.title = "Torrentio Results";
                unifiedContent.source = "Torrentio";

                for (const auto& stream : streams) {
                    if (stream.contains("infoHash")) {
                        TorrentQuality tq;
                        
                        std::string full_title = stream.value("title", "");
                        tq.quality = parseName(full_title);
                        tq.type = "torrentio";
                        tq.hash = stream.value("infoHash", "");
                        tq.size_bytes = parseSizeStr(full_title);
                        tq.seeders = parseSeeders(full_title);
                        tq.leechers = 0; // Not provided by Torrentio

                        // Assemble MagnetURI dynamically
                        std::ostringstream magnet;
                        magnet << "magnet:?xt=urn:btih:" << tq.hash 
                               << "&tr=udp://tracker.opentrackr.org:1337/announce"
                               << "&tr=udp://exodus.desync.com:6969"
                               << "&tr=udp://tracker.torrent.eu.org:451/announce";
                        tq.magnet_uri = magnet.str();

                        unifiedContent.torrents.push_back(tq);
                    }
                }

                if (!unifiedContent.torrents.empty()) {
                    results.push_back(unifiedContent);
                }

            } catch (const json::exception& e) {
                // Safely grab the first 100 characters of the response to see what's actually failing
                std::string raw_preview = json_response.length() > 100 ? json_response.substr(0, 100) + "..." : json_response;
                // Remove linebreaks from preview so it formats nicely in syslog
                raw_preview.erase(std::remove(raw_preview.begin(), raw_preview.end(), '\n'), raw_preview.end());
                raw_preview.erase(std::remove(raw_preview.begin(), raw_preview.end(), '\r'), raw_preview.end());

                spdlog::warn("Torrentio JSON parse error: {} | Raw Response: '{}'", e.what(), raw_preview);
            }

            return results;
        }
    };

    TorrentioClient::TorrentioClient() : m_impl(std::make_unique<Impl>()) {
        spdlog::info("Torrentio Client initialized");
    }

    TorrentioClient::~TorrentioClient() = default;

    std::vector<DiscoveredContent> TorrentioClient::fetchPopular(int /*limit*/, int /*page*/) {
        return {}; // Torrentio doesn't support generic 'popular' fetching without IMDB
    }

    std::vector<DiscoveredContent> TorrentioClient::search(const std::string& /*query*/, int /*limit*/, int /*page*/) {
        return {}; // Non-IMDB search not supported natively
    }

    std::optional<DiscoveredContent> TorrentioClient::getById(const std::string&) {
        return std::nullopt; // Managed via searchTorrentsByIMDB
    }

    std::vector<DiscoveredContent> TorrentioClient::searchTorrentsByIMDB(const std::string& imdb_id, const std::string& type, int season, int episode) {
        if (imdb_id.empty() || imdb_id.length() < 3) {
            spdlog::warn("TorrentioClient: Skipped fetch due to missing or invalid IMDB ID.");
            return {};
        }

        std::string url;
        if (type == "tv" && season > 0 && episode > 0) {
            url = m_impl->BASE_URL + "/stream/series/" + imdb_id + ":" + std::to_string(season) + ":" + std::to_string(episode) + ".json";
        } else {
            url = m_impl->BASE_URL + "/stream/movie/" + imdb_id + ".json";
        }

        spdlog::info("Torrentio Fetch: {}", url);
        std::string response = httpGetTimeout(url);

        // If Cloudflare or Rate-Limiting throws an HTTP 429/500/502, the response won't be JSON.
        return m_impl->parseStreams(response);
    }

} // namespace media::services
