#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

namespace media::api {

#include OATPP_CODEGEN_BEGIN(DTO)

    // Request Object: What the client sends us
    class AddTorrentDto : public oatpp::DTO {
        DTO_INIT(AddTorrentDto, DTO)

        DTO_FIELD(String, magnet_link);
        DTO_FIELD(Boolean, sequential, "sequential_download"); // Optional field
    };

    // Response Object: A single torrent's status
    class TorrentStatusDto : public oatpp::DTO {
        DTO_INIT(TorrentStatusDto, DTO)

        DTO_FIELD(String, name);
        DTO_FIELD(Float32, progress);
        DTO_FIELD(String, state);
        DTO_FIELD(Int32, download_rate);
    };

    // Generic Response
    class MessageDto : public oatpp::DTO {
        DTO_INIT(MessageDto, DTO)

        DTO_FIELD(Int32, status_code);
        DTO_FIELD(String, message);
    };

#include OATPP_CODEGEN_END(DTO)

} // namespace media::api