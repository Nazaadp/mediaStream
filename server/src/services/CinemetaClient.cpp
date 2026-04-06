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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
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

                for (const auto& meta : j["metas"]) {
                    DiscoveredContent content;
                    content.imdb_id = meta.value("id", "");
                    content.title = meta.value("name", "");
                    content.original_title = content.title;
                    
                    if (meta.contains("year") && meta["year"].is_string()) {
                        try {
                            content.year = std::stoi(meta["year"].get<std::string>());
                        } catch (...) { content.year = 0; }
                    } else if (meta.contains("releaseInfo") && meta["releaseInfo"].is_string()) {
                        try {
                            content.year = std::stoi(meta["releaseInfo"].get<std::string>().substr(0, 4));
                        } catch (...) { content.year = 0; }
                    }

                    content.poster_url = meta.value("poster", "");
                    if (meta.contains("description")) content.description = meta.value("description", "");
                    if (meta.contains("imdbRating")) {
                        try {
                            content.rating = std::stof(meta["imdbRating"].get<std::string>());
                        } catch (...) { content.rating = 0.0f; }
                    }

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

    std::vector<DiscoveredContent> CinemetaClient::fetchPopular(int) {
        spdlog::info("Fetching popular movies from Cinemeta...");
        // Fetch top movies
        std::string url = m_impl->BASE_URL + "/movie/top.json";
        std::string response = cmHttpGet(url);
        return m_impl->parseMetas(response, "Cinemeta");
    }

    std::vector<DiscoveredContent> CinemetaClient::fetchSeries(int) {
        spdlog::info("Fetching popular series from Cinemeta...");
        std::string url = m_impl->BASE_URL + "/series/top.json";
        std::string response = cmHttpGet(url);
        return m_impl->parseMetas(response, "Cinemeta");
    }

    std::vector<DiscoveredContent> CinemetaClient::search(const std::string& query, int) {
        // Cinemeta search endpoint is typically /catalog/movie/search={query}.json 
        // We will just do a basic implementation or return empty for this specific sprint phase since the user said 
        // focus on "retrieve popular movies/series/animes". We will implement movie search here to be thorough.
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

        std::string url = m_impl->BASE_URL + "/movie/search=" + url_query + ".json";
        return m_impl->parseMetas(cmHttpGet(url), "Cinemeta");
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
