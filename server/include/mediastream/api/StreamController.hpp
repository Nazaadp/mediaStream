#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "mediastream/core/TorrentEngine.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <string>
#include <filesystem>

namespace media::api {

#include OATPP_CODEGEN_BEGIN(ApiController)

class StreamController : public oatpp::web::server::api::ApiController {
private:
    std::shared_ptr<media::core::TorrentEngine> m_engine;

    // ?file=N → file index inside the torrent; -1 (absent/invalid) = largest.
    static int parseFileParam(const std::shared_ptr<IncomingRequest>& request) {
        auto fileParam = request->getQueryParameter("file");
        if (!fileParam) return -1;
        try { return std::stoi(fileParam->c_str()); } catch (...) { return -1; }
    }

public:
    StreamController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                     std::shared_ptr<media::core::TorrentEngine> engine)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_engine(engine) 
    {}

    // --- OPTIONS CORS endpoint (covers /stream/* including /stream/{hash}/info) ---
    ENDPOINT_INFO(optionsStreamPrefix) { info->summary = "CORS Preflight"; }
    ENDPOINT("OPTIONS", "/api/v1/stream/*", optionsStreamPrefix) {
        return createResponse(Status::CODE_204, "");
    }

    // --- GET /api/v1/stream/{infoHash}/info ---
    // Returns lightweight JSON metadata about the largest file in a torrent.
    // Used by the frontend for proactive codec detection (HEVC filename patterns +
    // MediaCapabilities.decodingInfo()) before playback starts, so users see the
    // codec warning immediately rather than after audio starts on a black screen.
    ENDPOINT_INFO(streamInfo) { info->summary = "Get file metadata for codec pre-check"; }
    ENDPOINT("GET", "/api/v1/stream/{infoHash}/info", streamInfo,
             PATH(String, infoHash),
             REQUEST(std::shared_ptr<IncomingRequest>, request))
    {
        std::string target_hash = infoHash->c_str();
        std::transform(target_hash.begin(), target_hash.end(), target_hash.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // ?file=N — season packs: report on the requested file, not the largest.
        auto file_path_opt = m_engine->getFilePath(target_hash, parseFileParam(request));
        if (!file_path_opt) {
            return createResponse(Status::CODE_404, "Metadata not yet available");
        }

        std::filesystem::path fsp(file_path_opt.value());
        std::string filename = fsp.filename().string();

        // Size may be 0 if still pre-allocated (sequential download hasn't written yet)
        std::error_code ec;
        uint64_t size_bytes = std::filesystem::file_size(fsp, ec);
        if (ec) size_bytes = 0;

        // Mirror the mime-type logic from streamVideo so front-end gets a consistent value
        std::string mime = "video/mp4";
        std::string ext  = fsp.extension().string();
        if (ext == ".mkv")  mime = "video/x-matroska";
        else if (ext == ".avi")  mime = "video/x-msvideo";
        else if (ext == ".webm") mime = "video/webm";

        // Minimal hand-rolled JSON — avoid pulling in nlohmann just for this tiny payload.
        // Filename is escaped to handle quotes/backslashes in torrent-provided names.
        std::string escaped_name;
        for (char c : filename) {
            if (c == '"' || c == '\\') escaped_name += '\\';
            escaped_name += c;
        }

        std::string json = "{\"filename\":\"" + escaped_name
                         + "\",\"size_bytes\":"  + std::to_string(size_bytes)
                         + ",\"mime_type\":\""   + mime + "\"}";

        auto response = createResponse(Status::CODE_200, json);
        response->putHeader("Content-Type", "application/json");
        return response;
    }

    ENDPOINT_INFO(streamVideo) {
        info->summary = "Stream video file with Range support";
    }
    ENDPOINT("GET", "/api/v1/stream/{infoHash}", streamVideo,
             PATH(String, infoHash), REQUEST(std::shared_ptr<IncomingRequest>, request))
    {
        oatpp::String range = request->getHeader("Range");

        std::string target_hash = infoHash->c_str();
        std::transform(target_hash.begin(), target_hash.end(), target_hash.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // ?file=N selects which file of a season pack to stream. Without it a
        // pack always resolved to its largest file — playing "episode 2" of a
        // pack silently streamed episode 1 (or whatever file was biggest).
        const int file_index = parseFileParam(request);

        auto file_path_opt = m_engine->getFilePath(target_hash, file_index);
        if (!file_path_opt) {
            return createResponse(Status::CODE_404, "File not found or metadata not downloaded");
        }

        std::string fp = file_path_opt.value();
        if (!std::filesystem::exists(fp)) {
            return createResponse(Status::CODE_404, "File does not exist yet mapping to torrent");
        }

        std::error_code ec;
        uint64_t file_size = std::filesystem::file_size(fp, ec);
        if (ec || file_size == 0) {
            return createResponse(Status::CODE_404, "File empty or cannot be read");
        }

        uint64_t start = 0;
        uint64_t end = file_size - 1;

        if (range && range->c_str()[0] != '\0') {
            std::string r = range->c_str();
            auto eq_idx = r.find('=');
            if (eq_idx != std::string::npos) {
                std::string bytes_range = r.substr(eq_idx + 1);
                auto dash_idx = bytes_range.find('-');
                if (dash_idx != std::string::npos) {
                    std::string start_str = bytes_range.substr(0, dash_idx);
                    std::string end_str = bytes_range.substr(dash_idx + 1);
                    
                    try {
                        if (!start_str.empty()) start = std::stoull(start_str);
                        if (!end_str.empty()) end = std::stoull(end_str);
                    } catch (const std::exception& e) {
                        spdlog::warn("Security: Malformed Range header payload: {}", e.what());
                        return createResponse(Status::CODE_400, "Invalid Range Format");
                    }
                }
            }
        }

        if (end < start) { end = start; }

        if (start >= file_size) {
            auto resp = createResponse(Status::CODE_416, "Requested Range Not Satisfiable");
            resp->putHeader("Content-Range", "bytes */" + std::to_string(file_size));
            return resp;
        }

        if (end >= file_size) {
            end = file_size - 1;
        }

        // Block and wait for the requested piece. If the piece is not yet downloaded
        // (timeout), return 503 Retry-After so the browser re-requests in 2 seconds
        // instead of receiving pre-allocated zeros from the file, which poisons the
        // browser's video decoder and causes videoHeight=0 on loadedmetadata.
        bool piece_ready = m_engine->waitForPiece(target_hash, start, file_index);
        if (!piece_ready) {
            auto response = createResponse(Status::CODE_503, "Piece not yet available");
            response->putHeader("Retry-After", "2");
            response->putHeader("Content-Type", "text/plain");
            return response;
        }

        uint64_t chunk_size = end - start + 1;
        
        // Limit chunk size to 4MB max for better streaming responsiveness
        uint64_t max_chunk = 4 * 1024 * 1024;
        if (chunk_size > max_chunk) {
            end = start + max_chunk - 1;
            chunk_size = max_chunk;
        }

        std::ifstream file(fp, std::ios::binary);
        if (!file) {
            return createResponse(Status::CODE_500, "Failed to open file");
        }

        file.seekg(start, std::ios::beg);
        auto buffer = oatpp::String(chunk_size);
        file.read(reinterpret_cast<char*>(buffer->data()), chunk_size);
        
        // If we read less than chunk_size (e.g. at the end or file updating)
        chunk_size = file.gcount();
        if (chunk_size == 0) {
            // Hitting the end of the currently downloaded piece in a growing file.
            // If we return 416, the browser thinks the entire video is finished and stops.
            // Instead, we return a 206 with 0 bytes to force the browser to gently ask again soon.
            // (Alternative: sleep and block here, but oatpp async handlers prefer returning).
            end = start;
            auto empty_buffer = oatpp::String("");
            auto response = createResponse(Status::CODE_206, empty_buffer);
            response->putHeader("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/*");
            response->putHeader("Accept-Ranges", "bytes");
            return response;
        }
        
        // Adjust end based on actual read bytes
        end = start + chunk_size - 1;
        buffer = oatpp::String((const char*)buffer->data(), chunk_size); 

        auto response = createResponse(Status::CODE_206, buffer);
        response->putHeader("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
        response->putHeader("Accept-Ranges", "bytes");
        
        // Determine mime type from extension
        std::string mime = "video/mp4";
        // MKV MIME type note: use video/x-matroska (not video/webm).
        // WebM is a restricted Matroska subset (VP8/VP9/AV1 only). Serving H.264-in-MKV
        // as video/webm makes Chromium activate its strict WebM demuxer, which rejects
        // the H.264 video track → audio plays but videoHeight stays 0.
        // video/x-matroska routes through the broader Matroska demuxer which handles H.264.
        if (fp.ends_with(".mkv")) mime = "video/x-matroska";
        else if (fp.ends_with(".avi")) mime = "video/x-msvideo";
        else if (fp.ends_with(".webm")) mime = "video/webm";
        response->putHeader("Content-Type", mime); 
        
        return response;
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
