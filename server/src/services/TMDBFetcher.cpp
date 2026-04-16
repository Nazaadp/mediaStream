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
                    url = BASE_URL + "/find/" + content.imdb_id + "?external_source=imdb_id&language=en-US";
                } else if (!content.title.empty()) {
                    std::string encoded_title = urlEncode(content.title);
                    url = BASE_URL + "/search/" + determined_type + "?query=" + encoded_title + "&language=en-US";
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
                    if (result.contains("title") && result["title"].is_string() && !result["title"].get<std::string>().empty()) {
                        content.title = result["title"].get<std::string>();
                    } else if (result.contains("name") && result["name"].is_string() && !result["name"].get<std::string>().empty()) {
                        content.title = result["name"].get<std::string>();
                    }

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

    std::vector<SeasonInfo> TMDBFetcher::fetchSeasons(const std::string& imdb_id) {
        std::vector<SeasonInfo> results;
        if (m_impl->m_api_key.empty() || imdb_id.empty()) return results;

        try {
            // Step 1: resolve IMDB ID → TMDB ID
            std::string find_url = m_impl->BASE_URL + "/find/" + imdb_id + "?external_source=imdb_id&language=en-US";
            std::string find_res = tmdbHttpGet(find_url, m_impl->m_api_key);
            if (find_res.empty()) return results;

            auto fj = json::parse(find_res);
            std::string tmdb_id;
            if (fj.contains("tv_results") && !fj["tv_results"].empty()) {
                tmdb_id = std::to_string(fj["tv_results"][0].value("id", 0));
            }
            if (tmdb_id.empty() || tmdb_id == "0") {
                spdlog::warn("fetchSeasons: no TMDB TV match for {}", imdb_id);
                return results;
            }

            // Step 2: fetch show details (includes seasons array)
            std::string show_url = m_impl->BASE_URL + "/tv/" + tmdb_id + "?language=en-US";
            std::string show_res = tmdbHttpGet(show_url, m_impl->m_api_key);
            if (show_res.empty()) return results;

            auto sj = json::parse(show_res);
            if (!sj.contains("seasons")) return results;

            const std::string IMG_BASE = "https://image.tmdb.org/t/p/w300";
            for (const auto& s : sj["seasons"]) {
                int sn = s.value("season_number", -1);
                if (sn < 1) continue; // skip specials (season 0)
                SeasonInfo si;
                si.season_number = sn;
                si.name = s.value("name", "Season " + std::to_string(sn));
                si.episode_count = s.value("episode_count", 0);
                si.rating = s.value("vote_average", 0.0f);
                if (s.contains("poster_path") && s["poster_path"].is_string()) {
                    si.poster_url = IMG_BASE + s["poster_path"].get<std::string>();
                }
                results.push_back(si);
            }
            spdlog::info("fetchSeasons: {} seasons for {}", results.size(), imdb_id);
        } catch (const std::exception& e) {
            spdlog::error("fetchSeasons failed for {}: {}", imdb_id, e.what());
        }
        return results;
    }

    std::vector<EpisodeInfo> TMDBFetcher::fetchEpisodes(const std::string& imdb_id, int season) {
        std::vector<EpisodeInfo> results;
        if (m_impl->m_api_key.empty() || imdb_id.empty()) return results;

        try {
            // Resolve IMDB → TMDB ID
            std::string find_url = m_impl->BASE_URL + "/find/" + imdb_id + "?external_source=imdb_id&language=en-US";
            std::string find_res = tmdbHttpGet(find_url, m_impl->m_api_key);
            if (find_res.empty()) return results;

            auto fj = json::parse(find_res);
            std::string tmdb_id;
            if (fj.contains("tv_results") && !fj["tv_results"].empty()) {
                tmdb_id = std::to_string(fj["tv_results"][0].value("id", 0));
            }
            if (tmdb_id.empty() || tmdb_id == "0") return results;

            // Fetch season episodes
            std::string ep_url = m_impl->BASE_URL + "/tv/" + tmdb_id + "/season/" + std::to_string(season) + "?language=en-US";
            std::string ep_res = tmdbHttpGet(ep_url, m_impl->m_api_key);
            if (ep_res.empty()) return results;

            auto ej = json::parse(ep_res);
            if (!ej.contains("episodes")) return results;

            const std::string STILL_BASE = "https://image.tmdb.org/t/p/w300";
            for (const auto& ep : ej["episodes"]) {
                EpisodeInfo ei;
                ei.episode_number = ep.value("episode_number", 0);
                ei.season_number = season;
                ei.name = ep.value("name", "Episode " + std::to_string(ei.episode_number));
                ei.overview = ep.value("overview", "");
                ei.rating = ep.value("vote_average", 0.0f);
                if (ep.contains("still_path") && ep["still_path"].is_string()) {
                    ei.still_url = STILL_BASE + ep["still_path"].get<std::string>();
                }
                results.push_back(ei);
            }
            spdlog::info("fetchEpisodes: {} episodes for {} S{}", results.size(), imdb_id, season);
        } catch (const std::exception& e) {
            spdlog::error("fetchEpisodes failed for {} S{}: {}", imdb_id, season, e.what());
        }
        return results;
    }

    std::string TMDBFetcher::fetchGenres(const std::string& imdb_id) {
        if (m_impl->m_api_key.empty() || imdb_id.empty()) return "";
        try {
            // Step 1: resolve IMDB ID → TMDB ID + type
            std::string find_url = m_impl->BASE_URL + "/find/" + imdb_id + "?external_source=imdb_id&language=en-US";
            std::string find_res = tmdbHttpGet(find_url, m_impl->m_api_key);
            if (find_res.empty()) return "";

            auto fj = json::parse(find_res);
            std::string tmdb_id;
            std::string media_type;
            if (fj.contains("movie_results") && !fj["movie_results"].empty()) {
                tmdb_id = std::to_string(fj["movie_results"][0].value("id", 0));
                media_type = "movie";
            } else if (fj.contains("tv_results") && !fj["tv_results"].empty()) {
                tmdb_id = std::to_string(fj["tv_results"][0].value("id", 0));
                media_type = "tv";
            }
            if (tmdb_id.empty() || tmdb_id == "0") return "";

            // Step 2: fetch full details — returns genres array with names
            std::string detail_url = m_impl->BASE_URL + "/" + media_type + "/" + tmdb_id + "?language=en-US";
            std::string detail_res = tmdbHttpGet(detail_url, m_impl->m_api_key);
            if (detail_res.empty()) return "";

            auto dj = json::parse(detail_res);
            if (!dj.contains("genres")) return "";

            std::string result;
            for (const auto& g : dj["genres"]) {
                if (!g.contains("name")) continue;
                if (!result.empty()) result += ", ";
                result += g["name"].get<std::string>();
            }
            spdlog::info("fetchGenres: {} for {}", result, imdb_id);
            return result;
        } catch (const std::exception& e) {
            spdlog::error("fetchGenres failed for {}: {}", imdb_id, e.what());
            return "";
        }
    }

} // namespace media::services
