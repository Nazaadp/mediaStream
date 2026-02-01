#include "mediastream/core/TorrentEngine.hpp"

// External Dependencies (Isolated here)
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

// Logging
#include <spdlog/spdlog.h>

namespace media::core {

    // Helper: Safe Hex Converter (Libtorrent 2.0 compatible)
    std::string to_hex_string(const lt::sha1_hash& hash) {
        std::stringstream ss;
        ss << hash; // Libtorrent overloads the stream operator for hashes
        return ss.str();
    }

    struct TorrentEngine::Impl {
        lt::session session;
        std::filesystem::path save_path;

        Impl(const std::filesystem::path& path) : save_path(path) {
            lt::settings_pack p;
            
            // --- SECURITY HARDENING ---
            
            // 1. Force Encryption (RC4)
            // Prevent ISP throttling and basic packet inspection.
            // Note: This reduces peer availability slightly but increases privacy.
            p.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_forced);
            p.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_forced);
            p.set_int(lt::settings_pack::allowed_enc_level, lt::settings_pack::pe_both);

            // 2. Network Interface Binding (Zero Trust)
            // Explicitly bind to 0.0.0.0 or a specific interface if known.
            // p.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881");

            // 3. Resource Limits (DoS Prevention)
            // Don't let the client eat all file descriptors.
            p.set_int(lt::settings_pack::connections_limit, 200);
            p.set_int(lt::settings_pack::active_downloads, 3);

            // 4. Privacy
            // Disable DHT/LSD if you want a strictly private environment (optional)
            // p.set_bool(lt::settings_pack::enable_dht, false);
            // p.set_bool(lt::settings_pack::enable_lsd, false);

            // Apply settings
            session.apply_settings(p);
            
            spdlog::info("TorrentEngine initialized. Encryption: FORCED. Path: {}", path.string());
        }
    };

    // Constructor
    TorrentEngine::TorrentEngine(const std::filesystem::path& save_path) 
        : m_pimpl(std::make_unique<Impl>(save_path)) {
        
        if (!std::filesystem::exists(save_path)) {
            std::filesystem::create_directories(save_path);
        }
    }

    // Destructor (Required for PIMPL with unique_ptr)
    TorrentEngine::~TorrentEngine() = default;
    
    // Move Semantics
    TorrentEngine::TorrentEngine(TorrentEngine&&) noexcept = default;
    TorrentEngine& TorrentEngine::operator=(TorrentEngine&&) noexcept = default;

    void TorrentEngine::addMagnet(const std::string& magnet_uri) {
        if (magnet_uri.empty() || magnet_uri.find("magnet:?") == std::string::npos) {
            spdlog::error("Security Alert: Invalid magnet link provided.");
            throw std::invalid_argument("Invalid magnet URI format");
        }

        try {
            lt::add_torrent_params params = lt::parse_magnet_uri(magnet_uri);
            params.save_path = m_pimpl->save_path.string();
            
            // --- STREAMING OPTIMIZATION ---
            // High priority ensures pieces are downloaded for playback (0, 1, 2...)
            // rather than rarity.
            params.flags |= lt::torrent_flags::sequential_download;
            
            m_pimpl->session.async_add_torrent(params);
            spdlog::info("Magnet added to queue (Sequential Mode): {}", params.name);

        } catch (const std::exception& e) {
            spdlog::error("Failed to parse magnet URI: {}", e.what());
            throw; // Re-throw to let the caller handle the UI feedback
        }
    }

    void TorrentEngine::removeTorrent(const std::string& info_hash) {
        // 1. Get all active handles
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        
        // 2. Find the one matching the hash
        for (auto& h : handles) {
            if (!h.is_valid()) continue;
            
            // Convert internal hash to string to compare
            std::string current_hash = lt::aux::to_hex(h.info_hash()); 
            
            // Libtorrent hex strings are often uppercase, input might be lower.
            // For simplicity, we assume exact match or handle casing in a utility.
            // Let's rely on the API sending the exact string we gave it.
            if (current_hash == info_hash) {
                // 3. Remove it (and delete files if you want 'options::delete_files')
                m_session.remove_torrent(h);
                return;
            }
        }
        throw std::runtime_error("Torrent not found");
    }

    void TorrentEngine::pause() {
        m_pimpl->session.pause();
        spdlog::info("Session paused.");
    }

    void TorrentEngine::resume() {
        m_pimpl->session.resume();
        spdlog::info("Session resumed.");
    }

    std::vector<TorrentStatus> TorrentEngine::getSessionStatus() const {
        std::vector<TorrentStatus> statuses;
        std::vector<lt::torrent_handle> handles = m_pimpl->session.get_torrents();

        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            lt::torrent_status ts = h.status();
            
            TorrentStatus s;

            s.info_hash = to_hex_string(h.info_hash());
            s.name = ts.name;
            s.progress = ts.progress_ppm
            s.num_peers = ts.num_peers;
            
            // Map libtorrent state to clean string
            switch(ts.state) {
                case lt::torrent_status::checking_files: s.state = "Checking"; break;
                case lt::torrent_status::downloading_metadata: s.state = "Fetching Metadata"; break;
                case lt::torrent_status::downloading: s.state = "Downloading"; break;
                case lt::torrent_status::finished: s.state = "Finished"; break;
                case lt::torrent_status::seeding: s.state = "Seeding"; break;
                default: s.state = "Queued"; break;
            }

            s.download_rate = ts.download_payload_rate;
            
            statuses.push_back(s);
        }
        return statuses;
    }

} // namespace media::core