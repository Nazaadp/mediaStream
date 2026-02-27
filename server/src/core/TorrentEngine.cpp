#include "mediastream/core/TorrentEngine.hpp"

// External Dependencies
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/hex.hpp>

#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>

// Define alias for this file only
namespace lt = libtorrent;

namespace media::core {

    // Helper: Safe Hex Converter
    std::string to_hex_string(const lt::sha1_hash& hash) {
        std::stringstream ss;
        ss << hash; 
        return ss.str();
    }

    // --- CONSTRUCTOR ---
    TorrentEngine::TorrentEngine(const std::filesystem::path& download_dir) 
        : m_download_dir(download_dir) {
        
        // 1. Create Directory if missing
        if (!std::filesystem::exists(m_download_dir)) {
            std::filesystem::create_directories(m_download_dir);
        }

        // 2. Configure Settings
        lt::settings_pack p;
        
        // Security: Force Encryption (RC4)
        p.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
        p.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
        p.set_int(lt::settings_pack::allowed_enc_level, lt::settings_pack::pe_both);

        // Resource Limits
        p.set_int(lt::settings_pack::connections_limit, 200);
        p.set_int(lt::settings_pack::active_downloads, 3);

        // 3. Apply Settings to Session
        m_session.apply_settings(p);
        
        spdlog::info("TorrentEngine initialized. Path: {}", m_download_dir.string());
    }

    // --- DESTRUCTOR ---
    TorrentEngine::~TorrentEngine() {
        // Session cleans up automatically
    }

    // --- ADD MAGNET ---
    void TorrentEngine::addMagnet(const std::string& magnet_uri) {
        if (magnet_uri.empty() || magnet_uri.find("magnet:?") == std::string::npos) {
            spdlog::error("Security Alert: Invalid magnet link provided.");
            throw std::invalid_argument("Invalid magnet URI format");
        }

        try {
            lt::add_torrent_params params = lt::parse_magnet_uri(magnet_uri);
            
            // Check if it already exists
            std::string incoming_hash = to_hex_string(params.info_hashes.get_best());
            std::vector<lt::torrent_handle> handles = m_session.get_torrents();
            for (const auto& h : handles) {
                if (h.is_valid() && to_hex_string(h.info_hash()) == incoming_hash) {
                    spdlog::info("Torrent already exists in session. Skiping add: {}", incoming_hash);
                    return; // Already downloading or seeding
                }
            }

            params.save_path = m_download_dir.string();
            
            // Streaming Optimization: Sequential Download
            params.flags |= lt::torrent_flags::sequential_download;
            
            m_session.async_add_torrent(params);
            spdlog::info("Magnet added to queue: {}", params.name);

        } catch (const std::exception& e) {
            spdlog::error("Failed to parse magnet URI: {}", e.what());
            throw; 
        }
    }

    // --- REMOVE TORRENT ---
    void TorrentEngine::removeTorrent(const std::string& info_hash_str) {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        
        for (auto& h : handles) {
            if (!h.is_valid()) continue;
            
            // Use our helper to get the hash string
            std::string current_hash = to_hex_string(h.info_hash()); 
            
            if (current_hash == info_hash_str) {
                m_session.remove_torrent(h);
                spdlog::info("Torrent removed: {}", info_hash_str);
                return;
            }
        }
        throw std::runtime_error("Torrent not found");
    }

    // --- GET STATUS ---
    std::vector<TorrentStatus> TorrentEngine::getSessionStatus() const {
        std::vector<TorrentStatus> statuses;
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();

        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            lt::torrent_status ts = h.status();
            TorrentStatus s;

            s.info_hash = to_hex_string(h.info_hash());
            s.name = ts.name;
            s.progress = ts.progress; // Raw float 0.0 - 1.0
            
            // Map state
            switch(ts.state) {
                case lt::torrent_status::checking_files: s.state = "Checking"; break;
                case lt::torrent_status::downloading_metadata: s.state = "Fetching Metadata"; break;
                case lt::torrent_status::downloading: s.state = "Downloading"; break;
                case lt::torrent_status::finished: s.state = "Finished"; break;
                case lt::torrent_status::seeding: s.state = "Seeding"; break;
                default: s.state = "Queued"; break;
            }

            s.download_rate = ts.download_payload_rate;
            // s.num_peers = ts.num_peers; // Uncomment if added to struct

            statuses.push_back(s);
        }
        return statuses;
    }

    // --- GET LARGEST FILE PATH ---
    std::optional<std::string> TorrentEngine::getLargestFilePath(const std::string& info_hash_str) const {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;
            
            if (to_hex_string(h.info_hash()) == info_hash_str) {
                if (!h.torrent_file()) return std::nullopt; // Metadata not yet downloaded
                
                auto finfo = h.torrent_file()->files();
                if (finfo.num_files() == 0) return std::nullopt;

                int largest_index = -1;
                int64_t largest_size = 0;

                for (int i = 0; i < finfo.num_files(); ++i) {
                    if (finfo.file_size(lt::file_index_t(i)) > largest_size) {
                        largest_size = finfo.file_size(lt::file_index_t(i));
                        largest_index = i;
                    }
                }

                if (largest_index != -1) {
                    std::filesystem::path full_path = m_download_dir;
                    full_path /= finfo.file_path(lt::file_index_t(largest_index));
                    return full_path.string();
                }
            }
        }
        return std::nullopt;
    }

} // namespace media::core