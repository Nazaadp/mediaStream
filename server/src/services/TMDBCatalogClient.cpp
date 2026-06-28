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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
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
                        content.type = "movie";
                    } else if (result.contains("name")) { // TV
                        content.title = result.value("name", "");
                        content.original_title = result.value("original_name", content.title);
                        if (result.contains("first_air_date") && !result["first_air_date"].get<std::string>().empty()) {
                            content.year = std::stoi(result["first_air_date"].get<std::string>().substr(0, 4));
                        }
                        content.type = "tv";
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
                    content.original_language = result.value("original_language", "");

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

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchPopular(int /*limit*/, int page, const std::string& genre, const std::string& language) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching popular movies from TMDB (page {}, genre {}, lang {})...", page, genre, language);
        
        std::string url = m_impl->BASE_URL + "/discover/movie?language=en-US&sort_by=popularity.desc&page=" + std::to_string(page);
        if (!genre.empty()) url += "&with_genres=" + genre;
        if (!language.empty()) url += "&with_original_language=" + language;

        std::string response = tmdbCatHttpGet(url, m_impl->m_api_key);
        spdlog::info("=== RAW RESPONSE [MOVIES / TMDB] {} ===\n{}\n=== END RAW RESPONSE [MOVIES / TMDB] ===", url, response);
        return m_impl->parseResults(response, "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchSeries(int /*limit*/, int page, const std::string& genre, const std::string& language) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching popular series from TMDB (page {}, genre {}, lang {})...", page, genre, language);

        std::string url = m_impl->BASE_URL + "/discover/tv?language=en-US&sort_by=popularity.desc&page=" + std::to_string(page);
        if (!genre.empty()) url += "&with_genres=" + genre;
        if (!language.empty()) url += "&with_original_language=" + language;

        std::string response = tmdbCatHttpGet(url, m_impl->m_api_key);
        spdlog::info("=== RAW RESPONSE [SERIES / TMDB] {} ===\n{}\n=== END RAW RESPONSE [SERIES / TMDB] ===", url, response);
        return m_impl->parseResults(response, "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::fetchAnime(int /*limit*/, int page, const std::string& genre, const std::string& language) {
        if (m_impl->m_api_key.empty()) return {};
        spdlog::info("Fetching anime from TMDB (page {}, genre {}, lang {})...", page, genre, language);
        
        std::string url = m_impl->BASE_URL + "/discover/tv?with_genres=16&with_original_language=ja&sort_by=popularity.desc&page=" + std::to_string(page);
        // Note: TMDB anime fetch uses fixed genre 16 and JA. We can append additional filters if needed.
        if (!genre.empty()) url += "&with_genres=" + genre;
        if (!language.empty()) url += "&with_original_language=" + language;

        std::string response = tmdbCatHttpGet(url, m_impl->m_api_key);
        spdlog::info("=== RAW RESPONSE [ANIME / TMDB] {} ===\n{}\n=== END RAW RESPONSE [ANIME / TMDB] ===", url, response);
        return m_impl->parseResults(response, "TMDB");
    }

    std::vector<DiscoveredContent> TMDBCatalogClient::search(const std::string& query, int /*limit*/, int page, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        if (m_impl->m_api_key.empty()) return {};
        
        CURL* curl = curl_easy_init();
        std::string url_query = query;
        if (curl) {
            // Only escape if it's not already looking like it's escaped (contains %)
            if (query.find('%') == std::string::npos) {
                char* output = curl_easy_escape(curl, query.c_str(), query.length());
                if (output) { url_query = output; curl_free(output); }
            }
            curl_easy_cleanup(curl);
        }

        std::vector<DiscoveredContent> all_results;
        
        // TMDB Multi-search is good but can be noisy. Let's stick with it for now 
        // but ensure the encoding is correct.
        std::string url = m_impl->BASE_URL + "/search/multi?query=" + url_query + "&language=en-US&page=" + std::to_string(page);
        auto results = m_impl->parseResults(tmdbCatHttpGet(url, m_impl->m_api_key), "TMDB");
        
        return results;
    }

    std::optional<DiscoveredContent> TMDBCatalogClient::getById(const std::string&) {
        // Not used explicitly by catalog yet
        return std::nullopt; 
    }

} // namespace media::services
