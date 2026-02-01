#pragma once

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
        // Constructor: define save_path where files will land
        explicit TorrentEngine(const std::filesystem::path& save_path);
        
        // Rule of 5: PIMPL requires a defined destructor in the .cpp
        ~TorrentEngine();
        TorrentEngine(TorrentEngine&&) noexcept;
        TorrentEngine& operator=(TorrentEngine&&) noexcept;
        
        // Delete copy to ensure unique ownership of the session
        TorrentEngine(const TorrentEngine&) = delete;
        TorrentEngine& operator=(const TorrentEngine&) = delete;

        // Core Actions
        // Throws std::invalid_argument if magnet link is malformed
        void addMagnet(const std::string& magnet_uri);
        void removeTorrent(const std::string& info_hash);
        
        void pause();
        void resume();
        
        // Returns snapshot of current state. 
        // In a real system, we might use a callback/observer pattern, 
        // but polling is acceptable for Phase 1.
        [[nodiscard]] std::vector<TorrentStatus> getSessionStatus() const;

    private:
        // Forward declaration of the implementation struct
        struct Impl;
        std::unique_ptr<Impl> m_pimpl;
        lt::session m_session;
        std::filesystem::path save_path;
    };

} // namespace media::core