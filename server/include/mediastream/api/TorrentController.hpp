#pragma once

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"

#include "mediastream/core/TorrentEngine.hpp"
#include "mediastream/api/DTOs.hpp"

#include <spdlog/spdlog.h>

namespace media::api {

// Code Gen Macro to enable Oat++ magic
#include OATPP_CODEGEN_BEGIN(ApiController)

class TorrentController : public oatpp::web::server::api::ApiController {
private:
    // We hold a reference to the Engine (Dependency Injection)
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
                 return createDtoResponse(Status::CODE_400, "Missing magnet_link");
            }

            // In a real app, we would parse JSON here. 
            // For now, assume the raw body is the magnet link.
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

        // Map Core Objects to API DTOs
        auto response_list = oatpp::Vector<oatpp::Object<TorrentStatusDto>>::createShared();
        

        for (const auto& item : engine_status) {
            auto dto = TorrentStatusDto::createShared();
            dto->name = item.name;
            dto->progress = item.progress;
            dto->state = item.state;
            dto->download_rate = item.download_rate;
            response_list->push_back(dto);
        }

        return createDtoResponse(Status::CODE_200, response_list);
    }

private:
    // Helper to create simple JSON responses
    std::shared_ptr<OutgoingResponse> createDtoResponse(Status status, const std::string& msg) {
        auto dto = MessageDto::createShared();
        dto->status_code = status.code;
        dto->message = msg;
        return createResponse(status, dto);
    }


};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api