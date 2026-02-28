#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "mediastream/database/Database.hpp"
#include "mediastream/api/DTOs.hpp" 
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace media::api {

#include OATPP_CODEGEN_BEGIN(ApiController)

class UserController : public oatpp::web::server::api::ApiController {
private:
    std::shared_ptr<media::database::Database> m_db;

    oatpp::String serializeMediaList(const std::vector<media::database::MediaItem>& content_list) {
        nlohmann::json j_arr = nlohmann::json::array();
        for (const auto& m : content_list) {
            nlohmann::json root;
            root["id"] = m.id;
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
            root["language"] = m.language;
            
            // Add view_later and specific watch history data if needed, 
            // for now, just pushing the media item.
            
            // We fetch torrents for this media item so the UI can play them again
            auto torrents = m_db->getTorrentsForMedia(m.id);
            root["torrents"] = nlohmann::json::array();
            for (const auto& t : torrents) {
                nlohmann::json tj;
                tj["quality"] = t.quality;
                tj["size_bytes"] = t.size_bytes;
                tj["hash"] = t.info_hash;
                tj["magnet_uri"] = t.magnet_uri;
                tj["seeders"] = t.seeders;
                tj["leechers"] = t.leechers;
                root["torrents"].push_back(tj);
            }
            j_arr.push_back(root);
        }
        return j_arr.dump();
    }

    media::database::MediaItem parseMediaItemFromJson(const nlohmann::json& j) {
        media::database::MediaItem m;
        m.title = j.value("title", "");
        m.original_title = j.value("original_title", "");
        m.year = j.value("year", 0);
        m.description = j.value("description", "");
        m.poster_url = j.value("poster_url", "");
        m.backdrop_url = j.value("backdrop_url", "");
        m.rating = j.value("rating", 0.0f);
        m.genres = j.value("genres", "");
        m.runtime_minutes = j.value("runtime_minutes", 0);
        m.imdb_id = j.value("imdb_id", "");
        m.tmdb_id = j.value("tmdb_id", "");
        m.language = j.value("language", "en");
        
        m.type = media::database::ContentType::MOVIE; // Default
        
        return m;
    }

public:
    UserController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                   std::shared_ptr<media::database::Database> db)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_db(db) 
    {}

    ENDPOINT_INFO(optionsUser) { info->summary = "CORS Preflight"; }
    ENDPOINT("OPTIONS", "/api/v1/user/*", optionsUser) { return createResponse(Status::CODE_204, ""); }

    // --- History ---
    ENDPOINT_INFO(postHistory) { info->summary = "Save Watch History"; }
    ENDPOINT("POST", "/api/v1/user/history", postHistory,
             BODY_STRING(String, body)) 
    {
        try {
            auto j = nlohmann::json::parse(body->c_str());
            if (!j.contains("media")) {
                return createResponse(Status::CODE_400, "Missing media object");
            }
            
            auto item = parseMediaItemFromJson(j["media"]);
            if (!j["media"].contains("type") && j.contains("type")) {
                 std::string typeStr = j.value("type", "MOVIE");
                 if (typeStr == "SERIES") item.type = media::database::ContentType::SERIES;
                 else if (typeStr == "ANIME") item.type = media::database::ContentType::ANIME;
            }

            // Upsert the media item first
            int media_id = m_db->upsertMediaItem(item);

            // UPSERT Torrent files so the History list has them
            if (j["media"].contains("torrents")) {
                for (const auto& t : j["media"]["torrents"]) {
                    media::database::TorrentInfo tinfo;
                    tinfo.media_id = media_id;
                    tinfo.info_hash = t.value("hash", "");
                    tinfo.magnet_uri = t.value("magnet_uri", "");
                    tinfo.quality = t.value("quality", "");
                    tinfo.size_bytes = t.value("size_bytes", 0LL);
                    tinfo.seeders = t.value("seeders", 0);
                    tinfo.leechers = t.value("leechers", 0);
                    tinfo.source = "Discovery"; // Fallback
                    tinfo.status = media::database::DownloadStatus::PENDING;
                    
                    try {
                        m_db->insertTorrent(tinfo);
                    } catch (...) { /* Likely already exists */ }
                }
            }

            // Save history
            media::database::WatchHistory history;
            history.media_id = media_id;
            history.position_seconds = j.value("position_seconds", 0);
            history.duration_seconds = j.value("duration_seconds", 0);
            history.progress_percent = j.value("progress_percent", 0.0f);
            history.completed = j.value("completed", false);
            
            m_db->upsertWatchHistory(history);

            auto response = createResponse(Status::CODE_200, "{\"success\":true}");
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_400, e.what());
        }
    }

    ENDPOINT_INFO(getHistory) { info->summary = "Get Watch History"; }
    ENDPOINT("GET", "/api/v1/user/history", getHistory) {
        try {
            auto media_list = m_db->getWatchHistoryMedia(20);
            auto response = createResponse(Status::CODE_200, serializeMediaList(media_list));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    // --- View Later ---
    ENDPOINT_INFO(postViewLater) { info->summary = "Toggle View Later"; }
    ENDPOINT("POST", "/api/v1/user/viewlater", postViewLater,
             BODY_STRING(String, body)) 
    {
        try {
            auto j = nlohmann::json::parse(body->c_str());
            if (!j.contains("media")) {
                return createResponse(Status::CODE_400, "Missing media object");
            }
            
            auto item = parseMediaItemFromJson(j["media"]);
            
            // Upsert the media item first
            int media_id = m_db->upsertMediaItem(item);

            // UPSERT Torrent files
            if (j["media"].contains("torrents")) {
                for (const auto& t : j["media"]["torrents"]) {
                    media::database::TorrentInfo tinfo;
                    tinfo.media_id = media_id;
                    tinfo.info_hash = t.value("hash", "");
                    tinfo.magnet_uri = t.value("magnet_uri", "");
                    tinfo.quality = t.value("quality", "");
                    tinfo.size_bytes = t.value("size_bytes", 0LL);
                    tinfo.seeders = t.value("seeders", 0);
                    tinfo.leechers = t.value("leechers", 0);
                    tinfo.source = "Discovery";
                    tinfo.status = media::database::DownloadStatus::PENDING;
                    
                    try {
                        m_db->insertTorrent(tinfo);
                    } catch (...) { /* Likely already exists */ }
                }
            }

            bool saved = true;
            if (j.contains("saved") && j["saved"].is_boolean()) {
                saved = j["saved"].get<bool>();
            }
            spdlog::info("Toggling ViewLater for media_id {} to {}", media_id, saved);
            m_db->toggleViewLater(media_id, saved);

            auto response = createResponse(Status::CODE_200, "{\"success\":true}");
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_400, e.what());
        }
    }

    ENDPOINT_INFO(getViewLater) { info->summary = "Get View Later"; }
    ENDPOINT("GET", "/api/v1/user/viewlater", getViewLater) {
        try {
            auto media_list = m_db->getViewLaterMedia(20);
            auto response = createResponse(Status::CODE_200, serializeMediaList(media_list));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
