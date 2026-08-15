#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/services/TorrentScorer.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

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

        spdlog::info("=== RAW RESPONSE [YTS] {} ===\n{}\n=== END RAW RESPONSE [YTS] ===", url, response);
        return response;
    }

    // YTS Client Implementation
    class YTSClient::Impl {
    public:
        const char* env_url = std::getenv("YTS_URL");
        const std::string BASE_URL = env_url ? env_url : "https://yts.bz/api/v2";

        std::vector<DiscoveredContent> parseMovieList(const std::string& json_response) {
            std::vector<DiscoveredContent> results;

            try {
                auto j = json::parse(json_response);
                
                if (j["status"] != "ok") {
                    spdlog::error("YTS API returned error status");
                    return results;
                }

                auto movies = j["data"]["movies"];
                
                for (const auto& movie : movies) {
                    DiscoveredContent content;
                    content.title = movie.value("title", "");
                    content.original_title = movie.value("title_english", content.title);
                    content.year = movie.value("year", 0);
                    content.description = movie.value("synopsis", "");
                    content.rating = movie.value("rating", 0.0f);
                    content.runtime_minutes = movie.value("runtime", 0);
                    content.imdb_id = movie.value("imdb_code", "");
                    content.language = movie.value("language", "en");
                    content.source = "YTS";

                    // Poster URLs
                    content.poster_url = movie.value("medium_cover_image", "");
                    content.backdrop_url = movie.value("background_image_original", "");

                    // Genres
                    if (movie.contains("genres") && movie["genres"].is_array()) {
                        std::string genres_str;
                        for (const auto& genre : movie["genres"]) {
                            if (!genres_str.empty()) genres_str += ", ";
                            genres_str += genre.get<std::string>();
                        }
                        content.genres = genres_str;
                    }

                    // Torrents
                    if (movie.contains("torrents") && movie["torrents"].is_array()) {
                        for (const auto& torrent : movie["torrents"]) {
                            TorrentQuality tq;
                            tq.quality = torrent.value("quality", "");
                            tq.type = torrent.value("type", "web");
                            tq.source = "YTS";
                            tq.title = content.title + " (" + std::to_string(content.year) + ") ["
                                       + tq.quality + "." + tq.type + "]";
                            tq.audio_languages    = isoToDisplayLang(content.language);
                            tq.subtitle_languages = "N/A";
                            tq.size_bytes = torrent.value("size_bytes", 0LL);
                            tq.hash = torrent.value("hash", "");
                            tq.seeders = torrent.value("seeds", 0);
                            tq.leechers = torrent.value("peers", 0);
                            // Structured path: YTS gives the year as JSON, so the
                            // § 13 name parser never runs on YTS titles.
                            tq.parsed_title = content.title;
                            tq.year         = content.year;

                            // Structured quality enrichment — YTS exposes codec
                            // and bit-depth as JSON fields (not in the title), so
                            // the structured fast-path beats name-parsing here.
                            TorrentScorer::enrichFromYTS(
                                tq,
                                tq.quality,
                                torrent.value("video_codec", ""),
                                tq.type,
                                torrent.value("bit_depth", ""));

                            // Build magnet URI
                            if (!tq.hash.empty()) {
                                std::ostringstream magnet;
                                magnet << "magnet:?xt=urn:btih:" << tq.hash
                                       << "&dn=" << content.title
                                       << "&tr=udp://open.demonii.com:1337/announce"
                                       << "&tr=udp://tracker.openbittorrent.com:80"
                                       << "&tr=udp://tracker.coppersurfer.tk:6969"
                                       << "&tr=udp://glotorrents.pw:6969/announce"
                                       << "&tr=udp://tracker.opentrackr.org:1337/announce"
                                       << "&tr=udp://torrent.gresille.org:80/announce"
                                       << "&tr=udp://p4p.arenabg.com:1337"
                                       << "&tr=udp://tracker.leechers-paradise.org:6969";
                                tq.magnet_uri = magnet.str();
                            }

                            content.torrents.push_back(tq);
                        }
                    }

                    results.push_back(content);
                }

            } catch (const json::exception& e) {
                spdlog::error("YTS JSON parsing error: {}", e.what());
            }

            return results;
        }
    };

    // Constructor/Destructor
    YTSClient::YTSClient() : m_impl(std::make_unique<Impl>()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        spdlog::info("YTS Client initialized");
    }

    YTSClient::~YTSClient() {
        curl_global_cleanup();
    }

    // Fetch Popular Movies
    std::vector<DiscoveredContent> YTSClient::fetchPopular(int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        std::string url = m_impl->BASE_URL + "/list_movies.json?limit=" + std::to_string(limit) 
                         + "&sort_by=download_count&order_by=desc";
        
        spdlog::info("Fetching popular movies from YTS...");
        std::string response = httpGet(url);
        
        if (response.empty()) {
            spdlog::error("Failed to fetch from YTS");
            return {};
        }

        return m_impl->parseMovieList(response);
    }

    // Search Movies
    std::vector<DiscoveredContent> YTSClient::search(const std::string& query, int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        std::string encoded_query = query; // TODO: URL encode
        std::string url = m_impl->BASE_URL + "/list_movies.json?query_term=" + encoded_query 
                         + "&limit=" + std::to_string(limit);
        
        spdlog::info("Searching YTS for: {}", query);
        std::string response = httpGet(url);
        
        if (response.empty()) {
            spdlog::error("Failed to search YTS");
            return {};
        }

        return m_impl->parseMovieList(response);
    }

    // Get Movie by ID
    std::optional<DiscoveredContent> YTSClient::getById(const std::string& id) {
        std::string url = m_impl->BASE_URL + "/movie_details.json?movie_id=" + id;
        
        spdlog::info("Fetching YTS movie ID: {}", id);
        std::string response = httpGet(url);
        
        if (response.empty()) {
            return std::nullopt;
        }

        auto results = m_impl->parseMovieList(response);
        if (!results.empty()) {
            return results[0];
        }

        return std::nullopt;
    }

} // namespace media::services