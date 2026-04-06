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
            root["language"] = m.language;
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

            auto movies = m_discovery->fetchMovies(15, page);
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

            auto series = m_discovery->fetchSeries(15, page);
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

            auto anime = m_discovery->fetchAnime(15, page);
            auto response = createResponse(Status::CODE_200, serializeToJson(anime));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("Discovery Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
