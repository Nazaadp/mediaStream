#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "mediastream/core/TorrentEngine.hpp"
#include "mediastream/api/DTOs.hpp" 
#include <spdlog/spdlog.h>

namespace media::api {

#include OATPP_CODEGEN_BEGIN(ApiController)

class TorrentController : public oatpp::web::server::api::ApiController {
private:
    std::shared_ptr<media::core::TorrentEngine> m_engine;

public:
    TorrentController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                      std::shared_ptr<media::core::TorrentEngine> engine)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_engine(engine) 
    {}

    // --- ENDPOINT 1: Add Magnet ---
    ENDPOINT_INFO(addTorrent) {
        info->summary = "Add a new Magnet Link";
        info->addConsumes<Object<AddTorrentDto>>("application/json");
        info->addResponse<Object<MessageDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<MessageDto>>(Status::CODE_400, "application/json");
    }
    ENDPOINT("POST", "/api/v1/torrents", addTorrent,
             BODY_DTO(Object<AddTorrentDto>, dto)) 
    {
        try {
            if (!dto->magnet_link) {
                 auto err = MessageDto::createShared();
                 err->status_code = 400;
                 err->message = "Missing magnet_link";
                 // Uses BUILT-IN createDtoResponse
                 return createDtoResponse(Status::CODE_400, err); 
            }

            spdlog::info("API: Adding magnet: {}", dto->magnet_link->c_str());
            m_engine->addMagnet(dto->magnet_link);
            
            auto response = MessageDto::createShared();
            response->status_code = 200;
            response->message = "Torrent added successfully";
            return createDtoResponse(Status::CODE_200, response);

        } catch (const std::exception& e) {
            spdlog::error("API Error: {}", e.what());
            auto err = MessageDto::createShared();
            err->status_code = 400;
            err->message = e.what();
            return createDtoResponse(Status::CODE_400, err);
        }
    }

    // --- ENDPOINT 2: Get Status ---
    ENDPOINT_INFO(getStatus) {
        info->summary = "Get list of active downloads";
        info->addResponse<List<Object<TorrentStatusDto>>>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/api/v1/status", getStatus) {
        auto engine_status = m_engine->getSessionStatus();
        
        auto response_list = oatpp::Vector<oatpp::Object<TorrentStatusDto>>::createShared();
        
        for (const auto& item : engine_status) {
            auto dto = TorrentStatusDto::createShared();
            dto->info_hash = item.info_hash;
            dto->name = item.name;
            dto->progress = item.progress;
            dto->state = item.state;
            dto->download_rate = item.download_rate;
            response_list->push_back(dto);
        }

        return createDtoResponse(Status::CODE_200, response_list);
    }

    // --- ENDPOINT 3: Remove Torrent ---
    ENDPOINT_INFO(removeTorrent) {
        info->summary = "Stop and remove a torrent";
        // Define a Path Parameter "{infoHash}"
        info->addParameter("infoHash", oatpp::swagger::Parameter::IN_PATH).required = true;
        info->addResponse<Object<MessageDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<MessageDto>>(Status::CODE_404, "application/json");
    }
    ENDPOINT("DELETE", "/api/v1/torrents/{infoHash}", removeTorrent,
             PATH(String, infoHash)) // <--- Capture URL variable
    {
        try {
            spdlog::info("API: Removing torrent: {}", infoHash->c_str());
            m_engine->removeTorrent(infoHash);
            
            auto response = MessageDto::createShared();
            response->status_code = 200;
            response->message = "Torrent removed";
            return createDtoResponse(Status::CODE_200, response);
        } catch (const std::exception& e) {
            spdlog::error("API Error: {}", e.what());
            auto err = MessageDto::createShared();
            err->status_code = 404;
            err->message = e.what();
            return createDtoResponse(Status::CODE_404, err);
        }
    }
    
    // !!! IMPORTANT: The custom createDtoResponse helper is DELETED.
    // We use the one inherited from ApiController.
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api