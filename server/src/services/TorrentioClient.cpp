#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/services/TorrentScorer.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>
#include <sstream>
#include <mutex>
#include <chrono>
#include <thread>

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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
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

        spdlog::info("=== RAW RESPONSE [Torrentio] {} ===\n{}\n=== END RAW RESPONSE [Torrentio] ===", url, response);
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

        // Extracts the source tracker name from the Torrentio title block.
        // Torrentio embeds it after the ⚙️ emoji (UTF-8: \xE2\x9A\x99\xEF\xB8\x8F)
        // Example title line: "👤 100 💾 1.4 GB ⚙️ YTS"
        std::string parseSource(const std::string& title) {
            // ⚙️ in UTF-8
            const std::string gear = "\xE2\x9A\x99\xEF\xB8\x8F";
            auto pos = title.find(gear);
            if (pos == std::string::npos) return "torrentio";

            size_t start = pos + gear.size();
            while (start < title.size() && (title[start] == ' ' || title[start] == '\t')) {
                start++;
            }
            size_t end = start;
            while (end < title.size() && title[end] != '\n' && title[end] != '\r') {
                end++;
            }
            std::string src = title.substr(start, end - start);
            // Trim trailing whitespace
            while (!src.empty() && (src.back() == ' ' || src.back() == '\t')) {
                src.pop_back();
            }
            return src.empty() ? "torrentio" : src;
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

                        // Torrentio's title block is multi-line. For series/anime,
                        // line 1 is frequently a season-pack or messy multi-language
                        // label while the real episode file lives in
                        // behaviorHints.filename. Prefer the filename as both the
                        // display title and the parse target; fall back to the
                        // title's first line when no filename is provided.
                        std::string release_name = parseName(full_title);
                        if (stream.contains("behaviorHints") &&
                            stream["behaviorHints"].contains("filename")) {
                            release_name = stream["behaviorHints"].value("filename", release_name);
                        }

                        tq.title   = release_name;
                        tq.quality = tq.title; // keep quality in sync for legacy consumers
                        tq.source  = parseSource(full_title);
                        tq.type    = tq.source;  // keep type in sync for legacy consumers
                        tq.hash = stream.value("infoHash", "");
                        tq.size_bytes = parseSizeStr(full_title);
                        tq.seeders = parseSeeders(full_title);
                        tq.leechers = 0; // Not provided by Torrentio
                        // Languages are parsed from the FULL title block: the textual
                        // lang tags (MULTi / Dual / ITA / AMZN …) may sit on a line
                        // other than the filename, so the whole block is the richest
                        // source. (The 🇬🇧/🇷🇺 flag emojis are ignored by the parser.)
                        tq.audio_languages    = parseTorrentAudioLangs(full_title);
                        tq.subtitle_languages = parseTorrentSubtitleLangs(full_title);

                        // enrich() parses resolution/codec/HDR/release type from the
                        // release name and rewrites the legacy quality string.
                        TorrentScorer::enrich(tq, release_name);

                        // Backstop from Torrentio's structured "name" field
                        // (e.g. "Torrentio\n4k DV | HDR10+"), which reliably
                        // encodes resolution + HDR/DV. Only fills gaps left by the
                        // release-name parse — never overrides richer data.
                        std::string name_field = stream.value("name", "");
                        if (!name_field.empty()) {
                            const std::string name_norm = TorrentScorer::normalize(name_field);
                            if (tq.resolution_p == 0) {
                                tq.resolution_p = TorrentScorer::parseResolution(name_norm);
                                if (tq.resolution_p > 0)
                                    tq.quality = std::to_string(tq.resolution_p) + "p";
                            }
                            bool n_hdr = false, n_hdr10 = false, n_dv = false;
                            TorrentScorer::parseHDR(name_norm, n_hdr, n_hdr10, n_dv);
                            tq.is_hdr   = tq.is_hdr   || n_hdr;
                            tq.is_hdr10 = tq.is_hdr10 || n_hdr10;
                            tq.is_dv    = tq.is_dv    || n_dv;
                        }

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

    std::vector<DiscoveredContent> TorrentioClient::fetchPopular([[maybe_unused]] int limit, [[maybe_unused]] int page, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        return {}; // Torrentio doesn't support generic 'popular' fetching without IMDB
    }

    std::vector<DiscoveredContent> TorrentioClient::search([[maybe_unused]] const std::string& query, [[maybe_unused]] int limit, [[maybe_unused]] int page, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
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

        // Cloudflare error 1015 Rate Limit protection.
        // During initial load, the frontend fetches ~150 items across Movies/Series/Anime/History.
        // We enforce a strict 500ms delay between requests (max 2 req/sec) to avoid the 75-req/minute ban.
        auto enforceRateLimit = []() {
            static std::mutex s_rate_limit_mutex;
            static auto s_last_request_time = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(s_rate_limit_mutex);
            auto now = std::chrono::steady_clock::now();
            auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_request_time).count();
            if (time_since_last < 500) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500 - time_since_last));
            }
            s_last_request_time = std::chrono::steady_clock::now();
        };

        std::string response;
        int max_retries = 3;
        for (int i = 0; i < max_retries; ++i) {
            enforceRateLimit();
            spdlog::info("Torrentio Fetch (Attempt {}): {}", i + 1, url);
            response = httpGetTimeout(url);

            if (response.empty() || response.find("error code: 1015") != std::string::npos) {
                spdlog::warn("Torrentio Fetch hit Cloudflare ban or empty response. Retrying locally...");
                std::this_thread::sleep_for(std::chrono::seconds(2)); // wait 2s before retry
                continue;
            }
            break; // Valid non-1015 response
        }

        // Parse result as usual
        return m_impl->parseStreams(response);
    }

} // namespace media::services
