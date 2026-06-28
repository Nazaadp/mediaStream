#include "mediastream/services/ContentDiscovery.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace media::services {

    static size_t CMWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static std::string cmHttpGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CMWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MediaStream/2.0");

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                spdlog::warn("Cinemeta CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_easy_cleanup(curl);
        }
        return response;
    }

    class CinemetaClient::Impl {
    public:
        const std::string BASE_URL = "https://v3-cinemeta.strem.io/catalog";

        std::vector<DiscoveredContent> parseMetas(const std::string& json_response, const std::string& source_tag) {
            std::vector<DiscoveredContent> results;
            if (json_response.empty()) return results;

            try {
                auto j = json::parse(json_response);
                if (!j.contains("metas") || !j["metas"].is_array()) return results;

                // Helper: returns "" for missing or non-string (including null) fields.
                auto safeStr = [](const json& obj, const std::string& key) -> std::string {
                    if (!obj.contains(key) || !obj[key].is_string()) return "";
                    return obj[key].get<std::string>();
                };

                for (const auto& meta : j["metas"]) {
                    DiscoveredContent content;
                    content.imdb_id = safeStr(meta, "id");
                    content.title   = safeStr(meta, "name");
                    content.original_title = content.title;

                    const std::string year_str = safeStr(meta, "year");
                    const std::string release_str = safeStr(meta, "releaseInfo");
                    if (!year_str.empty()) {
                        try { content.year = std::stoi(year_str); } catch (...) {}
                    } else if (!release_str.empty()) {
                        try { content.year = std::stoi(release_str.substr(0, 4)); } catch (...) {}
                    }

                    content.poster_url  = safeStr(meta, "poster");
                    content.description = safeStr(meta, "description");

                    const std::string rating_str = safeStr(meta, "imdbRating");
                    if (!rating_str.empty()) {
                        try { content.rating = std::stof(rating_str); } catch (...) {}
                    }

                    const std::string type_str = safeStr(meta, "type");
                    content.type = (type_str == "series") ? "tv" : "movie";

                    content.source = source_tag;
                    results.push_back(content);
                }
            } catch (const std::exception& e) {
                spdlog::error("Cinemeta JSON parsing error: {}", e.what());
            }

            return results;
        }
    };

    CinemetaClient::CinemetaClient() : m_impl(std::make_unique<Impl>()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        spdlog::info("Cinemeta Client initialized");
    }

    CinemetaClient::~CinemetaClient() {
        curl_global_cleanup();
    }

    std::vector<DiscoveredContent> CinemetaClient::fetchPopular(int limit, int page, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        spdlog::info("Fetching popular movies from Cinemeta (page {})...", page);
        int skip = (page - 1) * limit; // Map page directly to limit chunk size
        std::string url = m_impl->BASE_URL + "/movie/top";
        if (skip > 0) url += "/skip=" + std::to_string(skip);
        url += ".json";
        std::string response = cmHttpGet(url);
        spdlog::info("=== RAW RESPONSE [MOVIES / Cinemeta] {} ===\n{}\n=== END RAW RESPONSE [MOVIES / Cinemeta] ===", url, response);
        return m_impl->parseMetas(response, "Cinemeta");
    }

    std::vector<DiscoveredContent> CinemetaClient::fetchSeries(int limit, int page, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        spdlog::info("Fetching popular series from Cinemeta (page {})...", page);
        int skip = (page - 1) * limit;
        std::string url = m_impl->BASE_URL + "/series/top";
        if (skip > 0) url += "/skip=" + std::to_string(skip);
        url += ".json";
        std::string response = cmHttpGet(url);
        spdlog::info("=== RAW RESPONSE [SERIES / Cinemeta] {} ===\n{}\n=== END RAW RESPONSE [SERIES / Cinemeta] ===", url, response);
        return m_impl->parseMetas(response, "Cinemeta");
    }

    std::vector<DiscoveredContent> CinemetaClient::search(const std::string& query, int /*limit*/, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        CURL* curl = curl_easy_init();
        std::string url_query = query;
        if (curl) {
            char* output = curl_easy_escape(curl, query.c_str(), query.length());
            if (output) {
                url_query = output;
                curl_free(output);
            }
            curl_easy_cleanup(curl);
        }

        std::vector<DiscoveredContent> all_results;
        
        // Search Movies
        std::string movie_url = m_impl->BASE_URL + "/movie/top/search=" + url_query + ".json";
        auto movies = m_impl->parseMetas(cmHttpGet(movie_url), "Cinemeta");
        all_results.insert(all_results.end(), movies.begin(), movies.end());

        // Search Series
        std::string series_url = m_impl->BASE_URL + "/series/top/search=" + url_query + ".json";
        auto series = m_impl->parseMetas(cmHttpGet(series_url), "Cinemeta");
        all_results.insert(all_results.end(), series.begin(), series.end());
        
        return all_results;
    }

    std::optional<DiscoveredContent> CinemetaClient::getById(const std::string& id) {
        std::string url = "https://v3-cinemeta.strem.io/meta/movie/" + id + ".json";
        std::string response = cmHttpGet(url);
        try {
            auto j = json::parse(response);
            if (j.contains("meta")) {
                std::string meta_arr = "{\"metas\":[" + j["meta"].dump() + "]}";
                auto res = m_impl->parseMetas(meta_arr, "Cinemeta");
                if (!res.empty()) return res[0];
            }
        } catch (...) {}
        return std::nullopt;
    }

} // namespace media::services
