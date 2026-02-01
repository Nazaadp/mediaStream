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

namespace media::core {

    struct TorrentStatus {
        std::string info_hash;
        std::string name;
        float progress; // 0.0 to 1.0
        int download_rate; // bytes per second
        int num_peers;
        std::string state; // "Downloading", "Seeding", "Checking"
    };

    class TorrentEngine {
    public:
        // Constructor
        explicit TorrentEngine(const std::filesystem::path& save_path);
        
        // Destructor
        ~TorrentEngine();

        // Core Actions
        void addMagnet(const std::string& magnet_uri);
        void removeTorrent(const std::string& info_hash);
        
        // Status
        [[nodiscard]] std::vector<TorrentStatus> getSessionStatus() const;

    private:
        // --- 2. DIRECT IMPLEMENTATION (Matches your .cpp) ---
        libtorrent::session m_session; 
        std::filesystem::path save_path; // Renamed to match your .cpp
    };

} // namespace media::core