#pragma once

// --- 1. CRITICAL INCLUDES ---
#include <libtorrent/session.hpp>       // <--- REQUIRED for m_session
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>
// ----------------------------

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <set>
#include <mutex>

namespace media::core {

    struct TorrentStatus {
        std::string info_hash;
        std::string name;
        float progress; // 0.0 to 1.0
        int download_rate; // bytes per second
        int num_peers;
        std::string state; // "Downloading", "Seeding", "Checking"
        // File metadata populated once torrent info is available.
        // Empty strings / 0 while in the "Fetching Metadata" phase.
        std::string filename;  // Basename of the largest file e.g. "Movie.x265.mkv"
        int64_t size_bytes{0}; // Pre-allocated file size from torrent metadata
        std::string mime_type; // "video/x-matroska", "video/mp4", etc.
    };

    class TorrentEngine {
    public:
        // Constructor
        explicit TorrentEngine(const std::filesystem::path& m_download_dir);
        
        // Destructor
        ~TorrentEngine();

        // Core Actions
        void addMagnet(const std::string& magnet_uri);
        void removeTorrent(const std::string& info_hash);
        
        // Status
        [[nodiscard]] std::vector<TorrentStatus> getSessionStatus() const;

        // Streaming Support
        [[nodiscard]] std::optional<std::string> getLargestFilePath(const std::string& info_hash) const;
        // Returns true if the piece was available or became available within the wait window.
        // Returns false if the timeout elapsed and the piece is NOT yet downloaded.
        // Callers MUST check the return value and NOT serve data on false (zeros in pre-alloc).
        [[nodiscard]] bool waitForPiece(const std::string& info_hash, uint64_t file_offset);

        // Called every second by the WS broadcaster.
        // Pre-fetches the last 5% of pieces (moov atom location) for every actively
        // downloading torrent so the moov is ready well before the user clicks Stream.
        void proactivelyBoostEndPieces();

    private:
        libtorrent::session m_session;
        std::filesystem::path m_download_dir;
        // Tracks torrents whose end-pieces have already been priority-boosted
        // with deadlines by proactivelyBoostEndPieces(). One-shot per torrent.
        std::set<std::string> m_moov_boosted;
        // Tracks (hash:piece_index) pairs for which deadlines have already been
        // submitted via waitForPiece(). Prevents re-setting deadlines on every
        // 503 retry from the frontend, which resets libtorrent's countdown and
        // can actively slow down out-of-order piece delivery.
        std::set<std::string> m_moov_deadline_set;
        // Guards m_moov_boosted and m_moov_deadline_set. These sets are mutated
        // from three thread contexts with no other synchronization: the WS
        // broadcaster (proactivelyBoostEndPieces), concurrent oatpp HTTP workers
        // (waitForPiece), and removeTorrent. Concurrent std::set mutation is UB.
        std::mutex m_moov_mutex;
    };

} // namespace media::core