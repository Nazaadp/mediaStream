#include "mediastream/services/TMDBFetcher.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdlib>

using json = nlohmann::json;

namespace media::services {

    static size_t TMDBWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static std::string tmdbHttpGet(const std::string& url, const std::string& api_key) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            std::string auth_header = "Authorization: Bearer " + api_key;
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "accept: application/json");
            headers = curl_slist_append(headers, auth_header.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TMDBWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); 
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                spdlog::error("TMDB CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        return response;
    }

    // Helper: URL encoding
    static std::string urlEncode(const std::string& value) {
        CURL* curl = curl_easy_init();
        if (curl) {
            char* output = curl_easy_escape(curl, value.c_str(), value.length());
            if (output) {
                std::string result(output);
                curl_free(output);
                curl_easy_cleanup(curl);
                return result;
            }
            curl_easy_cleanup(curl);
        }
        return value;
    }

    class TMDBFetcher::Impl {
    public:
        std::string m_api_key;
        const std::string BASE_URL = "https://api.themoviedb.org/3";

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
                spdlog::warn("TMDB_APIKEY_AT not found. TMDB integration will be disabled.");
            }
        }

        void enrichContent(DiscoveredContent& content) {
            if (m_api_key.empty()) return;

            std::string TMDB_IMAGE_BASE = "https://image.tmdb.org/t/p/w500";
            std::string TMDB_BACKDROP_BASE = "https://image.tmdb.org/t/p/original";
            
            try {
                std::string url;
                std::string determined_type = content.type.empty() ? ((content.source == "EZTV" || content.source == "Nyaa") ? "tv" : "movie") : content.type;

                // Fix entirely missing IMDB IDs by doing an external ID reverse lookup natively FIRST!
                if (content.imdb_id.empty() && !content.tmdb_id.empty()) {
                    std::string ext_url = BASE_URL + "/" + determined_type + "/" + content.tmdb_id + "/external_ids";
                    std::string ext_res = tmdbHttpGet(ext_url, m_api_key);
                    try {
                        auto ext_j = json::parse(ext_res);
                        if (ext_j.contains("imdb_id") && ext_j["imdb_id"].is_string()) {
                            content.imdb_id = ext_j["imdb_id"].get<std::string>();
                        }
                    } catch(...) {}
                }

                if (!content.imdb_id.empty()) {
                    url = BASE_URL + "/find/" + content.imdb_id + "?external_source=imdb_id";
                } else if (!content.title.empty()) {
                    std::string encoded_title = urlEncode(content.title);
                    url = BASE_URL + "/search/" + determined_type + "?query=" + encoded_title;
                } else {
                    return;
                }

                std::string response = tmdbHttpGet(url, m_api_key);
                if (response.empty()) return;

                auto j = json::parse(response);
                json result;

                if (j.contains("movie_results") && !j["movie_results"].empty()) {
                    result = j["movie_results"][0];
                } else if (j.contains("tv_results") && !j["tv_results"].empty()) {
                    result = j["tv_results"][0];
                } else if (j.contains("results") && !j["results"].empty()) {
                    result = j["results"][0];
                }

                if (!result.is_null()) {
                    if (result.contains("overview") && result["overview"].is_string() && !result["overview"].get<std::string>().empty()) {
                        content.description = result["overview"].get<std::string>();
                    }
                    if (result.contains("poster_path") && result["poster_path"].is_string()) {
                        content.poster_url = TMDB_IMAGE_BASE + result["poster_path"].get<std::string>();
                    }
                    if (result.contains("backdrop_path") && result["backdrop_path"].is_string()) {
                        content.backdrop_url = TMDB_BACKDROP_BASE + result["backdrop_path"].get<std::string>();
                    }
                    if (result.contains("vote_average") && result["vote_average"].is_number()) {
                        content.rating = result["vote_average"].get<float>();
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to enrich content from TMDB: {}", e.what());
            }
        }
    };

    TMDBFetcher::TMDBFetcher() : m_impl(std::make_unique<Impl>()) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    TMDBFetcher::~TMDBFetcher() {
    }

    void TMDBFetcher::enrichContent(DiscoveredContent& content) {
        m_impl->enrichContent(content);
    }

} // namespace media::services
