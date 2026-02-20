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

public:
    StreamController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                     std::shared_ptr<media::core::TorrentEngine> engine)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_engine(engine) 
    {}

    ENDPOINT_INFO(streamVideo) {
        info->summary = "Stream video file with Range support";
    }
    ENDPOINT("GET", "/api/v1/stream/{infoHash}", streamVideo,
             PATH(String, infoHash), HEADER(String, range, "Range")) 
    {
        auto file_path_opt = m_engine->getLargestFilePath(infoHash);
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
                    
                    if (!start_str.empty()) start = std::stoull(start_str);
                    if (!end_str.empty()) end = std::stoull(end_str);
                }
            }
        }

        if (start >= file_size) {
            auto resp = createResponse(Status::CODE_416, "Requested Range Not Satisfiable");
            resp->putHeader("Content-Range", "bytes */" + std::to_string(file_size));
            return resp;
        }

        if (end >= file_size) {
            end = file_size - 1;
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
            // Probably hitting end of currently downloaded piece
            // Serve whatever we have, or wait? Let's just return what we got.
            // If length is 0, return 416
            return createResponse(Status::CODE_416, "Requested Range Not Satisfiable currently");
        }
        
        // Adjust end based on actual read bytes
        end = start + chunk_size - 1;
        buffer = oatpp::String((const char*)buffer->data(), chunk_size); 

        auto response = createResponse(Status::CODE_206, buffer);
        response->putHeader("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
        response->putHeader("Accept-Ranges", "bytes");
        response->putHeader("Content-Length", std::to_string(chunk_size));
        
        // Determine mime type from extension
        std::string mime = "video/mp4";
        if (fp.ends_with(".mkv")) mime = "video/x-matroska";
        else if (fp.ends_with(".avi")) mime = "video/x-msvideo";
        else if (fp.ends_with(".webm")) mime = "video/webm";
        response->putHeader("Content-Type", mime); 
        
        return response;
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
