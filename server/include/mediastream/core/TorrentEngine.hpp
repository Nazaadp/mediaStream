#pragma once

// --- 1. CRITICAL INCLUDES ---
#include <libtorrent/session.hpp>       // <--- REQUIRED for m_session
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/version.hpp>
// ----------------------------

#include <map>
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
        std::string filename;  // Basename of the target file e.g. "Movie.x265.mkv"
        int64_t size_bytes{0}; // Pre-allocated file size from torrent metadata
        std::string mime_type; // "video/x-matroska", "video/mp4", etc.
        // Which file inside the torrent this entry reports on. -1 = whole
        // torrent / largest file (single-file torrents, legacy behavior).
        // Season packs emit one entry PER requested file, each with per-file
        // progress, so the client can badge episode 1 and 2 independently.
        int file_index{-1};
    };

    class TorrentEngine {
    public:
        // Constructor
        explicit TorrentEngine(const std::filesystem::path& m_download_dir);

        // Destructor
        ~TorrentEngine();

        // Core Actions.
        // file_index >= 0 selects one file inside a multi-file torrent (season
        // pack): only that file (plus any other requested files of the same
        // torrent) is downloaded. -1 keeps the legacy whole-torrent behavior.
        // Re-adding an existing torrent with a NEW file_index enables that
        // file's download instead of being a no-op.
        void addMagnet(const std::string& magnet_uri, int file_index = -1);
        // Removes the torrent and its files from the live session.
        // With file_index >= 0 and other files of the torrent still wanted,
        // only that file stops downloading (data of other files is kept).
        // Returns false when the hash is not in the session (e.g. it was added
        // before a restart) — caller may fall back to deleteDownloadedData().
        bool removeTorrent(const std::string& info_hash, int file_index = -1);
        // Out-of-session cleanup: deletes the downloaded data that contains
        // file_path (as persisted in the DB). Removes the torrent's whole
        // top-level entry under the download dir; refuses paths outside it.
        bool deleteDownloadedData(const std::string& file_path);

        // Status
        [[nodiscard]] std::vector<TorrentStatus> getSessionStatus() const;

        // Streaming Support.
        // file_index -1 → largest file (legacy). Valid index → that file.
        [[nodiscard]] std::optional<std::string> getFilePath(const std::string& info_hash, int file_index) const;
        [[nodiscard]] std::optional<std::string> getLargestFilePath(const std::string& info_hash) const;
        // Returns true if the piece was available or became available within the wait window.
        // Returns false if the timeout elapsed and the piece is NOT yet downloaded.
        // Callers MUST check the return value and NOT serve data on false (zeros in pre-alloc).
        // file_offset is relative to the TARGET FILE (file_index semantics above).
        [[nodiscard]] bool waitForPiece(const std::string& info_hash, uint64_t file_offset, int file_index = -1);

        // Called every second by the WS broadcaster.
        // Applies pending per-file priorities (metadata arrives async) and
        // pre-fetches the last 5% of pieces OF EACH TARGET FILE (moov atom
        // location) so the moov is ready well before the user clicks Stream.
        void proactivelyBoostEndPieces();

    private:
        // Applies dont_download/default priorities for a torrent according to
        // m_wanted_files. No-op until metadata is available or when the
        // torrent has no explicit wanted set (legacy whole-torrent mode).
        void applyFilePriorities(const libtorrent::torrent_handle& h, const std::string& hash);

        libtorrent::session m_session;
        std::filesystem::path m_download_dir;
        // Tracks (hash:file) pairs whose end-pieces have already been
        // priority-boosted with deadlines by proactivelyBoostEndPieces().
        // One-shot per file.
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
        // hash → set of file indices explicitly requested for download
        // (season packs). Empty/absent = whole-torrent legacy mode.
        std::map<std::string, std::set<int>> m_wanted_files;
        // Torrents whose current wanted-set has been applied to libtorrent.
        // Cleared whenever the wanted-set changes so the proactive loop
        // re-applies on its next tick.
        std::set<std::string> m_priorities_applied;
        // (hash:file) pairs already bumped to top_priority by an active
        // stream (waitForPiece) — the file being watched outranks prefetches.
        std::set<std::string> m_stream_boosted;
        // Guards m_wanted_files / m_priorities_applied / m_stream_boosted
        // (HTTP workers + WS broadcaster). mutable: getSessionStatus is const.
        mutable std::mutex m_files_mutex;
    };

} // namespace media::core