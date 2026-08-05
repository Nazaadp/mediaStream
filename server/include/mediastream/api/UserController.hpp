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
            root["original_language"] = m.original_language;
            root["type"] = media::database::contentTypeToString(m.type);
            
            // Add view_later and specific watch history data if needed, 
            // for now, just pushing the media item.
            
            // We fetch torrents for this media item so the UI can play them again
            auto torrents = m_db->getTorrentsForMedia(m.id);
            root["torrents"] = nlohmann::json::array();
            for (const auto& t : torrents) {
                root["torrents"].push_back(serializeTorrent(t));
            }
            j_arr.push_back(root);
        }
        return j_arr.dump();
    }

    nlohmann::json serializeTorrent(const media::database::TorrentInfo& t) {
        nlohmann::json tj;
        tj["quality"] = t.quality;
        tj["title"] = t.title;
        tj["type"] = t.type;
        tj["source"] = t.source;
        tj["size_bytes"] = t.size_bytes;
        tj["hash"] = t.info_hash;
        tj["magnet_uri"] = t.magnet_uri;
        tj["seeders"] = t.seeders;
        tj["leechers"] = t.leechers;
        // Which file inside the torrent this row refers to (-1 = largest) —
        // resuming episode 2 of a season pack must not re-open episode 1.
        tj["file_index"] = t.file_index;
        tj["is_pack"] = t.is_pack;
        // Only meaningful for series/anime episode torrents — 0 means "not
        // episode-bound", which the client expects as an absent field.
        if (t.season > 0) tj["season"] = t.season;
        if (t.episode > 0) tj["episode"] = t.episode;
        return tj;
    }

    // The client serializes absent optionals as JSON null (not missing keys),
    // and nlohmann's value() throws on null — extract defensively.
    media::database::TorrentInfo parseTorrentFromJson(const nlohmann::json& t, int media_id) {
        media::database::TorrentInfo tinfo;
        tinfo.media_id = media_id;
        tinfo.info_hash = jsonStr(t, "hash");
        tinfo.magnet_uri = jsonStr(t, "magnet_uri");
        tinfo.quality = jsonStr(t, "quality");
        tinfo.type = jsonStr(t, "type");
        tinfo.size_bytes = (t.contains("size_bytes") && t["size_bytes"].is_number())
            ? t["size_bytes"].get<int64_t>() : 0LL;
        tinfo.seeders = jsonInt(t, "seeders");
        tinfo.leechers = jsonInt(t, "leechers");
        tinfo.source = jsonStr(t, "source", "Discovery");
        tinfo.status = media::database::DownloadStatus::PENDING;
        tinfo.title = jsonStr(t, "title");
        tinfo.season = jsonInt(t, "season");
        tinfo.episode = jsonInt(t, "episode");
        tinfo.file_index = jsonInt(t, "file_index", -1);
        tinfo.is_pack = t.contains("is_pack") && t["is_pack"].is_boolean()
            && t["is_pack"].get<bool>();
        return tinfo;
    }

    // History-item shape shared by GET /user/history and GET /user/media_for_torrent:
    // the media item plus its stored torrents and resume/"Continue Watching" data.
    nlohmann::json serializeMediaWithHistory(const media::database::MediaItem& m) {
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
        root["original_language"] = m.original_language;
        root["type"] = media::database::contentTypeToString(m.type);

        auto torrents = m_db->getTorrentsForMedia(m.id);
        root["torrents"] = nlohmann::json::array();
        for (const auto& t : torrents) {
            root["torrents"].push_back(serializeTorrent(t));
        }

        auto wh = m_db->getWatchHistory(m.id);
        if (wh) {
            root["position_seconds"] = wh->position_seconds;
            root["duration_seconds"] = wh->duration_seconds;
            root["progress_percent"] = wh->progress_percent;
            root["completed"] = wh->completed;
            if (wh->last_season > 0) root["last_season"] = wh->last_season;
            if (wh->last_episode > 0) root["last_episode"] = wh->last_episode;
        }
        return root;
    }

    // Null-safe field extraction: nlohmann's value(key, default) substitutes
    // the default only for MISSING keys and throws type_error.302 on explicit
    // JSON null. Older clients serialize absent optionals as null, and one
    // null field must not void the whole save.
    static std::string jsonStr(const nlohmann::json& j, const char* key, const char* def = "") {
        return (j.contains(key) && j[key].is_string()) ? j[key].get<std::string>() : std::string(def);
    }
    static int jsonInt(const nlohmann::json& j, const char* key, int def = 0) {
        return (j.contains(key) && j[key].is_number()) ? j[key].get<int>() : def;
    }
    static float jsonFloat(const nlohmann::json& j, const char* key, float def = 0.0f) {
        return (j.contains(key) && j[key].is_number()) ? j[key].get<float>() : def;
    }

    media::database::MediaItem parseMediaItemFromJson(const nlohmann::json& j) {
        media::database::MediaItem m;
        m.title = jsonStr(j, "title");
        m.original_title = jsonStr(j, "original_title");
        m.year = jsonInt(j, "year");
        m.description = jsonStr(j, "description");
        m.poster_url = jsonStr(j, "poster_url");
        m.backdrop_url = jsonStr(j, "backdrop_url");
        m.rating = jsonFloat(j, "rating");
        m.genres = jsonStr(j, "genres");
        m.runtime_minutes = jsonInt(j, "runtime_minutes");
        m.imdb_id = jsonStr(j, "imdb_id");
        m.tmdb_id = jsonStr(j, "tmdb_id");
        m.language = jsonStr(j, "language", "en");
        m.original_language = jsonStr(j, "original_language");

        std::string typeStr = jsonStr(j, "type", "MOVIE");
        if (typeStr == "ANIME") {
            m.type = media::database::ContentType::ANIME;
        } else if (typeStr == "SERIES") {
            m.type = media::database::ContentType::SERIES;
        } else if (typeStr == "tv") {
            // TMDB/Cinemeta-style: distinguish anime by original_language
            const auto& lang = m.original_language;
            if (lang == "ja" || lang == "ko" || lang == "zh") {
                m.type = media::database::ContentType::ANIME;
            } else {
                m.type = media::database::ContentType::SERIES;
            }
        } else {
            // 'movie', 'MOVIE', or unknown → MOVIE
            m.type = media::database::ContentType::MOVIE;
        }

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

            // UPSERT Torrent files so the History list has them (existing rows
            // get their title/season/episode backfilled on re-play)
            if (j["media"].contains("torrents") && j["media"]["torrents"].is_array()) {
                for (const auto& t : j["media"]["torrents"]) {
                    try {
                        m_db->insertTorrent(parseTorrentFromJson(t, media_id));
                    } catch (...) { /* Insert/upsert failed — non-fatal */ }
                }
            }

            // Save history. Null-safe reads: a movie payload carries no episode
            // pointer, and "last_season": null must not abort the save.
            media::database::WatchHistory history;
            history.media_id = media_id;
            history.position_seconds = static_cast<int64_t>(jsonFloat(j, "position_seconds"));
            history.duration_seconds = static_cast<int64_t>(jsonFloat(j, "duration_seconds"));
            history.progress_percent = jsonFloat(j, "progress_percent");
            history.completed = j.contains("completed") && j["completed"].is_boolean()
                ? j["completed"].get<bool>() : false;
            history.last_season = jsonInt(j, "last_season");
            history.last_episode = jsonInt(j, "last_episode");

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

            // Each item: media + stored torrents + resume/"Continue Watching"
            // pointer so the client can render progress and resume playback.
            nlohmann::json j_arr = nlohmann::json::array();
            for (const auto& m : media_list) {
                j_arr.push_back(serializeMediaWithHistory(m));
            }
            auto response = createResponse(Status::CODE_200, oatpp::String(j_arr.dump()));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    // --- Torrent ↔ media association ---

    ENDPOINT_INFO(getMediaForTorrent) { info->summary = "Media item owning a torrent hash"; }
    ENDPOINT("GET", "/api/v1/user/media_for_torrent", getMediaForTorrent,
             QUERY(String, hash))
    {
        try {
            auto torrent = m_db->getTorrentByHash(std::string(hash->c_str()));
            if (!torrent) {
                return createResponse(Status::CODE_404, "{\"error\":\"unknown torrent\"}");
            }
            auto media = m_db->getMediaItem(torrent->media_id);
            if (!media) {
                return createResponse(Status::CODE_404, "{\"error\":\"no media for torrent\"}");
            }
            auto response = createResponse(
                Status::CODE_200, oatpp::String(serializeMediaWithHistory(*media).dump()));
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_500, "Internal Server Error");
        }
    }

    ENDPOINT_INFO(postUserTorrent) { info->summary = "Associate a torrent with a media item"; }
    ENDPOINT("POST", "/api/v1/user/torrents", postUserTorrent,
             BODY_STRING(String, body))
    {
        try {
            auto j = nlohmann::json::parse(body->c_str());
            if (!j.contains("media") || !j.contains("torrent")) {
                return createResponse(Status::CODE_400, "Missing media or torrent object");
            }
            // Persist the link WITHOUT creating a watch-history entry — used by
            // the "Download only" flow so a later play from the downloads
            // dropdown can resolve which media a torrent belongs to.
            int media_id = m_db->upsertMediaItem(parseMediaItemFromJson(j["media"]));
            m_db->insertTorrent(parseTorrentFromJson(j["torrent"], media_id));

            auto response = createResponse(Status::CODE_200, "{\"success\":true}");
            response->putHeader("Content-Type", "application/json");
            return response;
        } catch (const std::exception& e) {
            spdlog::error("User API Error: {}", e.what());
            return createResponse(Status::CODE_400, e.what());
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
            if (j["media"].contains("torrents") && j["media"]["torrents"].is_array()) {
                for (const auto& t : j["media"]["torrents"]) {
                    try {
                        m_db->insertTorrent(parseTorrentFromJson(t, media_id));
                    } catch (...) { /* Insert/upsert failed — non-fatal */ }
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
