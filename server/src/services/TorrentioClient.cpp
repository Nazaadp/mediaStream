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
            std::regex size_regex(R"(💾\s*([\d.]+)\s*([KMGT]?i?B))", std::regex_constants::icase);
            std::smatch match;
            if (std::regex_search(title, match, size_regex)) {
                double num = std::stod(match[1].str());
                std::string unit = match[2].str();
                std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);
                
                int64_t multiplier = 1;
                if (unit.find("K") != std::string::npos) multiplier = 1024LL;
                else if (unit.find("M") != std::string::npos) multiplier = 1024LL * 1024LL;
                else if (unit.find("G") != std::string::npos) multiplier = 1024LL * 1024LL * 1024LL;
                else if (unit.find("T") != std::string::npos) multiplier = 1024LL * 1024LL * 1024LL * 1024LL;

                return static_cast<int64_t>(num * multiplier);
            }
            return 0;
        }

        int parseSeeders(const std::string& title) {
            std::regex seeders_regex(R"(👤\s*(\d+))");
            std::smatch match;
            if (std::regex_search(title, match, seeders_regex)) {
                try {
                    return std::stoi(match[1].str());
                } catch (...) { return 0; }
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
                spdlog::error("Torrentio JSON parse error: {}", e.what());
            }

            return results;
        }
    };

    TorrentioClient::TorrentioClient() : m_impl(std::make_unique<Impl>()) {
        spdlog::info("Torrentio Client initialized");
    }

    TorrentioClient::~TorrentioClient() = default;

    std::vector<DiscoveredContent> TorrentioClient::fetchPopular(int) {
        return {}; // Torrentio doesn't support generic 'popular' fetching without IMDB
    }

    std::vector<DiscoveredContent> TorrentioClient::search(const std::string&, int) {
        return {}; // Non-IMDB search not supported natively
    }

    std::optional<DiscoveredContent> TorrentioClient::getById(const std::string&) {
        return std::nullopt; // Managed via searchTorrentsByIMDB
    }

    std::vector<DiscoveredContent> TorrentioClient::searchTorrentsByIMDB(const std::string& imdb_id, const std::string& type, int season, int episode) {
        std::string url;
        if (type == "tv" && season > 0 && episode > 0) {
            url = m_impl->BASE_URL + "/stream/series/" + imdb_id + ":" + std::to_string(season) + ":" + std::to_string(episode) + ".json";
        } else {
            url = m_impl->BASE_URL + "/stream/movie/" + imdb_id + ".json";
        }

        spdlog::info("Torrentio Fetch: {}", url);
        std::string response = httpGetTimeout(url);
        return m_impl->parseStreams(response);
    }

} // namespace media::services
