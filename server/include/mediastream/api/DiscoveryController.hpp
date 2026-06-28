#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "mediastream/services/ContentDiscovery.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace media::api {

#include OATPP_CODEGEN_BEGIN(ApiController)

class DiscoveryController : public oatpp::web::server::api::ApiController {
private:
    std::shared_ptr<media::services::ContentDiscoveryManager> m_discovery;

    oatpp::String serializeToJson(const std::vector<media::services::DiscoveredContent>& content_list) {
        nlohmann::json j_arr = nlohmann::json::array();
        for (const auto& m : content_list) {
            nlohmann::json root;
            root["title"] = m.title;
            root["original_title"] = m.original_title;
            root["year"] = m.year;
            root["description"] = m.description;
            root["poster_url"] = m.poster_url;
            root["backdrop_url"] = m.backdrop_url;
            root["rating"] = m.rating;
            root["genres"] = m.genres;
            root["runtime_minutes"] = m.runtime_minutes;
            root["imdb_id"] = m.imdb_id;
            root["tmdb_id"] = m.tmdb_id;
            root["type"] = m.type;
            root["language"] = m.language;
            root["original_language"] = m.original_language;
            root["source"] = m.source;
            root["torrents"] = nlohmann::json::array();
            for (const auto& t : m.torrents) {
                nlohmann::json tj;
                tj["quality"] = t.quality;
                tj["type"] = t.type;
                tj["size_bytes"] = t.size_bytes;
                tj["hash"] = t.hash;
                tj["magnet_uri"] = t.magnet_uri;
                tj["seeders"] = t.seeders;
                tj["leechers"] = t.leechers;
                root["torrents"].push_back(tj);
            }
            j_arr.push_back(root);
        }
        return j_arr.dump();
    }

public:
    DiscoveryController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                        std::shared_ptr<media::services::ContentDiscoveryManager> discovery)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_discovery(discovery) 
    {}

    ENDPOINT_INFO(optionsPreflight) {
        info->summary = "CORS Preflight";
    }
    ENDPOINT("OPTIONS", "/api/v1/discover/*", optionsPreflight) {
        return createResponse(Status::CODE_204, "");
    }

    ENDPOINT_INFO(getMovies) {
        info->summary = "Get popular movies";
    }
    ENDPOINT("GET", "/api/v1/discover/movies", getMovies, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            int page = 1;
            auto p = request->getQueryParameter("page");
            if (p) page = std::stoi(p->c_str());

            auto g = request->getQueryParameter("genre");
            auto l = request->getQueryParameter("language");
            std::string genre = g ? g->c_str() : "";
            std::string language = l ? l->c_str() : "";

            auto movies = m_discovery->fetchMovies(15, page, genre, language);
            auto response = createResponse(Status::CODE_200, serializeToJson(movies));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("Discovery Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getSeries) {
        info->summary = "Get popular series";
    }
    ENDPOINT("GET", "/api/v1/discover/series", getSeries, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            int page = 1;
            auto p = request->getQueryParameter("page");
            if (p) page = std::stoi(p->c_str());

            auto g = request->getQueryParameter("genre");
            auto l = request->getQueryParameter("language");
            std::string genre = g ? g->c_str() : "";
            std::string language = l ? l->c_str() : "";

            auto series = m_discovery->fetchSeries(15, page, genre, language);
            auto response = createResponse(Status::CODE_200, serializeToJson(series));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("Discovery Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getAnime) {
        info->summary = "Get popular anime";
    }
    ENDPOINT("GET", "/api/v1/discover/anime", getAnime, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            int page = 1;
            auto p = request->getQueryParameter("page");
            if (p) page = std::stoi(p->c_str());

            auto g = request->getQueryParameter("genre");
            auto l = request->getQueryParameter("language");
            std::string genre = g ? g->c_str() : "";
            std::string language = l ? l->c_str() : "";

            auto anime = m_discovery->fetchAnime(15, page, genre, language);
            auto response = createResponse(Status::CODE_200, serializeToJson(anime));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("Discovery Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }
    ENDPOINT_INFO(getMovieTorrents) {
        info->summary = "Get torrents for a movie by IMDB ID";
    }
    ENDPOINT("GET", "/api/v1/discover/movie_torrents", getMovieTorrents, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto id = request->getQueryParameter("imdb_id");
            if (!id) return createResponse(Status::CODE_400, "Missing imdb_id");

            auto torrents = m_discovery->fetchMovieTorrents(id->c_str());

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : torrents) {
                nlohmann::json tj;
                tj["quality"]             = t.quality;
                tj["type"]               = t.type;
                tj["title"]              = t.title;
                tj["source"]             = t.source;
                tj["audio_languages"]    = t.audio_languages;
                tj["subtitle_languages"] = t.subtitle_languages;
                tj["size_bytes"]         = t.size_bytes;
                tj["hash"]               = t.hash;
                tj["magnet_uri"]         = t.magnet_uri;
                tj["seeders"]            = t.seeders;
                tj["leechers"]           = t.leechers;
                // Parsed quality metadata (TorrentScorer) for client badges + sort
                tj["resolution_p"]       = t.resolution_p;
                tj["codec"]              = t.codec;
                tj["is_hdr"]             = t.is_hdr;
                tj["is_hdr10"]           = t.is_hdr10;
                tj["is_dv"]              = t.is_dv;
                tj["score"]              = t.score;
                arr.push_back(tj);
            }
            auto response = createResponse(Status::CODE_200, arr.dump());
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("getMovieTorrents error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getSeasons) {
        info->summary = "Get season list for a series by IMDB ID";
    }
    ENDPOINT("GET", "/api/v1/discover/seasons", getSeasons, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto id = request->getQueryParameter("imdb_id");
            if (!id) return createResponse(Status::CODE_400, "Missing imdb_id");

            auto seasons = m_discovery->fetchSeasons(id->c_str());

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : seasons) {
                nlohmann::json sj;
                sj["season_number"]  = s.season_number;
                sj["name"]           = s.name;
                sj["episode_count"]  = s.episode_count;
                sj["poster_url"]     = s.poster_url;
                sj["rating"]         = s.rating;
                arr.push_back(sj);
            }
            auto response = createResponse(Status::CODE_200, arr.dump());
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("getSeasons error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getGenres) {
        info->summary = "Get genre list for a title by IMDB ID";
    }
    ENDPOINT("GET", "/api/v1/discover/genres", getGenres, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto id = request->getQueryParameter("imdb_id");
            if (!id) return createResponse(Status::CODE_400, "Missing imdb_id");

            auto genres = m_discovery->fetchGenres(id->c_str());

            nlohmann::json obj;
            obj["genres"] = genres;
            auto response = createResponse(Status::CODE_200, obj.dump());
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("getGenres error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getEpisodes) {
        info->summary = "Get episode list for a series season by IMDB ID";
    }
    ENDPOINT("GET", "/api/v1/discover/episodes", getEpisodes, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto id = request->getQueryParameter("imdb_id");
            auto sn = request->getQueryParameter("season");
            if (!id || !sn) return createResponse(Status::CODE_400, "Missing imdb_id or season");

            auto episodes = m_discovery->fetchEpisodes(id->c_str(), std::stoi(sn->c_str()));

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& ep : episodes) {
                nlohmann::json ej;
                ej["episode_number"] = ep.episode_number;
                ej["season_number"]  = ep.season_number;
                ej["name"]           = ep.name;
                ej["overview"]       = ep.overview;
                ej["still_url"]      = ep.still_url;
                ej["rating"]         = ep.rating;
                arr.push_back(ej);
            }
            auto response = createResponse(Status::CODE_200, arr.dump());
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("getEpisodes error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(getEpisodeTorrents) {
        info->summary = "Get torrents for a specific episode";
    }
    ENDPOINT("GET", "/api/v1/discover/episode_torrents", getEpisodeTorrents, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto id = request->getQueryParameter("imdb_id");
            auto sn = request->getQueryParameter("season");
            auto ep = request->getQueryParameter("episode");
            if (!id || !sn || !ep) return createResponse(Status::CODE_400, "Missing imdb_id, season, or episode");

            auto t = request->getQueryParameter("title");
            std::string title = t ? t->c_str() : "";
            auto torrents = m_discovery->fetchEpisodeTorrents(
                id->c_str(), std::stoi(sn->c_str()), std::stoi(ep->c_str()), title);

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : torrents) {
                nlohmann::json tj;
                tj["quality"]             = t.quality;
                tj["type"]               = t.type;
                tj["title"]              = t.title;
                tj["source"]             = t.source;
                tj["audio_languages"]    = t.audio_languages;
                tj["subtitle_languages"] = t.subtitle_languages;
                tj["size_bytes"]         = t.size_bytes;
                tj["hash"]               = t.hash;
                tj["magnet_uri"]         = t.magnet_uri;
                tj["seeders"]            = t.seeders;
                tj["leechers"]           = t.leechers;
                // Parsed quality metadata (TorrentScorer) for client badges + sort
                tj["resolution_p"]       = t.resolution_p;
                tj["codec"]              = t.codec;
                tj["is_hdr"]             = t.is_hdr;
                tj["is_hdr10"]           = t.is_hdr10;
                tj["is_dv"]              = t.is_dv;
                tj["score"]              = t.score;
                arr.push_back(tj);
            }
            auto response = createResponse(Status::CODE_200, arr.dump());
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("getEpisodeTorrents error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(searchContent) {
        info->summary = "Search global catalog";
    }
    ENDPOINT("GET", "/api/v1/discover/search", searchContent, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        try {
            auto q = request->getQueryParameter("query");
            std::string query = q ? q->c_str() : "";

            if (query.empty()) {
                return createResponse(Status::CODE_400, "Missing query string");
            }

            auto results = m_discovery->searchAll(query, 15);
            auto response = createResponse(Status::CODE_200, serializeToJson(results));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("Discovery Search Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
