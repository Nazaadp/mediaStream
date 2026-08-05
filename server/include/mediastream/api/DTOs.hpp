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
        // Season packs: which file inside the torrent to download.
        // Absent/-1 = whole torrent (single-file torrents, legacy clients).
        DTO_FIELD(Int32, file_index);
    };

    // Response Object: A single torrent's status
    class TorrentStatusDto : public oatpp::DTO {
        DTO_INIT(TorrentStatusDto, DTO)

        DTO_FIELD(String, info_hash);
        DTO_FIELD(String, name);
        DTO_FIELD(Float32, progress);
        DTO_FIELD(String, state);
        DTO_FIELD(Int32, download_rate);
        // File metadata for proactive codec detection on the frontend.
        // Populated once torrent metadata is available (may be null before that).
        DTO_FIELD(String, filename);   // Target file basename, e.g. "Movie.x265.mkv"
        DTO_FIELD(Int64, size_bytes);  // Pre-allocated file size from torrent metadata
        DTO_FIELD(String, mime_type);  // video/x-matroska, video/mp4, etc.
        // Which file inside the torrent this entry reports on (-1 = whole
        // torrent / largest file). Season packs emit one entry per file.
        DTO_FIELD(Int32, file_index);
    };

    // Generic Response
    class MessageDto : public oatpp::DTO {
        DTO_INIT(MessageDto, DTO)

        DTO_FIELD(Int32, status_code);
        DTO_FIELD(String, message);
    };

#include OATPP_CODEGEN_END(DTO)

} // namespace media::api