#include "mediastream/services/ContentDiscovery.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdlib>

using json = nlohmann::json;

namespace media::services {

    static size_t TMDB_Cat_WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static std::string tmdbCatHttpGet(const std::string& url, const std::string& api_key) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            std::string auth_header = "Authorization: Bearer " + api_key;
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "accept: application/json");
            headers = curl_slist_append(headers, auth_header.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TMDB_Cat_WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); 
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                spdlog::error("TMDBCatalog CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        return response;
    }

    class TMDBCatalogClient::Impl {
    public:
        std::string m_api_key;
        const std::string BASE_URL = "https://api.themoviedb.org/3";
        const std::string TMDB_IMAGE_BASE = "https://image.tmdb.org/t/p/w500";
        const std::string TMDB_BACKDROP_BASE = "https://image.tmdb.org/t/p/original";

        Impl() {
            if (const char* env_p = std::getenv("TMDB_APIKEY_AT")) {
                m_api_key = env_p;
            } else {
                std::ifstream env_file(".env");
                if (env_file.is_open()) {
                    std::string line;
                    while (std::getline(env_file, line)) {
                        if (line.rfind("TMDB_APIKEY_AT=", 0) == 0) {
                            m_api_key = line.substr(15);
                            break;
                        }
                    }
                }
            }
            if (m_api_key.empty()) {
                spdlog::warn("TMDB_APIKEY_AT missing. TMDBCatalogClient disabled.");
            }
        }

        std::vector<DiscoveredContent> parseResults(const std::string& json_response, const std::string& source_tag) {
            std::vector<DiscoveredContent> items;
            if (json_response.empty() || m_api_key.empty()) return items;

            try {
                auto j = json::parse(json_response);
                if (!j.contains("results") || !j["results"].is_array()) return items;

                for (const auto& result : j["results"]) {
                    DiscoveredContent content;
                    
                    content.tmdb_id = std::to_string(result.value("id", 0));
                    
                    if (result.contains("title")) { // Movie
                        content.title = result.value("title", "");
                        content.original_title = result.value("original_title", content.title);
                        if (result.contains("release_date") && !result["release_date"].get<std::string>().empty()) {
                            content.year = std::stoi(result["release_date"].get<std::string>().substr(0, 4));
                        }
                    } else if (result.contains("name")) { // TV
                        content.title = result.value("name", "");
                        content.original_title = result.value("original_name", content.title);
                        if (result.contains("first_air_date") && !result["first_air_date"].get<std::string>().empty()) {
                            content.year = std::stoi(result["first_air_date"].get<std::string>().substr(0, 4));
                        }
                    }

                    if (result.contains("overview")) content.description = result.value("overview", "");
                    if (result.contains("poster_path") && result["poster_path"].is_string()) {
                        content.poster_url = TMDB_IMAGE_BASE + result["poster_path"].get<std::string>();
                    }
                    if (result.contains("backdrop_path") && result["backdrop_path"].is_string()) {
                        content.backdrop_url = TMDB_BACKDROP_BASE + result["backdrop_path"].get<std::string>();
                    }
                    if (result.contains("vote_average") && result["vote_average"].is_number()) {
                        content.rating = result.value("vote_average", 0.0f);
                    }

                    content.source = source_tag;
                    items.push_back(content);
                }
            } catch (const std::exception& e) {
                spdlog::error("TMDB json map error: {}", e.what());
            }

            return items;
        }
    };

    TMDBCatalogClient::TMDBCatalogClient() : m_impl(std::make_unique<Impl>()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        spdlog::info("TMDB Catalog Client initialized");
    }

    TMDBCatalogClient::~TMDBCatalogClient() {
        curl_global_cleanup();
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchPopular(int) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching popular movies from TMDB...");
        std::string url = m_impl->BASE_URL + "/movie/popular?language=en-US&page=1";
        return m_impl->parseResults(tmdbCatHttpGet(url, m_impl->m_api_key), "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchSeries(int) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching popular series from TMDB...");
        std::string url = m_impl->BASE_URL + "/tv/popular?language=en-US&page=1";
        return m_impl->parseResults(tmdbCatHttpGet(url, m_impl->m_api_key), "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchAnime(int) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching anime from TMDB...");
        std::string url = m_impl->BASE_URL + "/discover/tv?with_genres=16&with_original_language=ja&sort_by=popularity.desc";
        return m_impl->parseResults(tmdbCatHttpGet(url, m_impl->m_api_key), "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::search(const std::string& query, int) {
        if (m_impl->m_api_key.empty()) return {};
        CURL* curl = curl_easy_init();
        std::string url_query = query;
        if (curl) {
            char* output = curl_easy_escape(curl, query.c_str(), query.length());
            if (output) { url_query = output; curl_free(output); }
            curl_easy_cleanup(curl);
        }
        std::string url = m_impl->BASE_URL + "/search/multi?query=" + url_query + "&language=en-US&page=1";
        return m_impl->parseResults(tmdbCatHttpGet(url, m_impl->m_api_key), "TMDB");
    }

    std::optional<DiscoveredContent> TMDBCatalogClient::getById(const std::string&) {
        // Not used explicitly by catalog yet
        return std::nullopt; 
    }

} // namespace media::services
