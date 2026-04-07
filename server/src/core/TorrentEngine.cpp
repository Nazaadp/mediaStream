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
#include <thread>
#include <chrono>
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
    static int findLargestFileIndex(const lt::file_storage& finfo) {
        if (finfo.num_files() == 0) return -1;
        int largest_index = -1;
        int64_t largest_size = 0;
        for (int i = 0; i < finfo.num_files(); ++i) {
            if (finfo.file_size(lt::file_index_t(i)) > largest_size) {
                largest_size = finfo.file_size(lt::file_index_t(i));
                largest_index = i;
            }
        }
        return largest_index;
    }
    
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
                    spdlog::info("Torrent already exists in session. Skipping add: {}", incoming_hash);
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
                int largest_index = findLargestFileIndex(finfo);

                if (largest_index != -1) {
                    std::filesystem::path full_path = m_download_dir;
                    full_path /= finfo.file_path(lt::file_index_t(largest_index));
                    return full_path.string();
                }
            }
        }
        return std::nullopt;
    }

    // --- WAIT FOR PIECE ---
    // Returns true if the requested piece became available within the timeout.
    // Returns false if the torrent was not found, metadata not ready, or timeout elapsed.
    bool TorrentEngine::waitForPiece(const std::string& info_hash_str, uint64_t file_offset) {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;
            
            if (to_hex_string(h.info_hash()) == info_hash_str) {
                if (!h.torrent_file()) return false; // Metadata not yet downloaded
                
                auto finfo = h.torrent_file()->files();
                int largest_index = findLargestFileIndex(finfo);

                if (largest_index != -1) {
                    // Calculate the absolute byte offset within the torrent
                    int64_t torrent_offset = finfo.file_offset(lt::file_index_t(largest_index)) + file_offset;
                    // Calculate piece index
                    int piece_length = h.torrent_file()->piece_length();
                    if (piece_length <= 0) return false;
                    
                    lt::piece_index_t piece_idx(torrent_offset / piece_length);

                    // If piece index is beyond total pieces, return
                    if (piece_idx >= lt::piece_index_t(h.torrent_file()->num_pieces())) return false;

                    // Fast path: if already downloaded, return immediately
                    if (h.have_piece(piece_idx)) return true;

                    // --- MOOV ATOM DETECTION ---
                    // The browser seeks to the end of the file to find the MP4 moov atom
                    // (or MKV segment index). This seek lands on a piece far beyond the
                    // current sequential download position. We detect this by checking
                    // if the requested piece is in the last 15% of the torrent.
                    // When detected: boost ALL remaining end pieces (not just the target)
                    // so libtorrent delivers the full metadata block, not just one piece.
                    // Also extend the wait window to 4000ms to give libtorrent time to
                    // fulfill the out-of-order request from a peer connection.
                    const int total_pieces = h.torrent_file()->num_pieces();
                    const int piece_idx_int = static_cast<int>(piece_idx);
                    const bool is_moov_seek = piece_idx_int > (total_pieces * 85 / 100);

                    if (is_moov_seek) {
                        // Prioritize every piece from piece_idx to end-of-file.
                        // This ensures the full moov atom is available, not just one slice.
                        spdlog::info(
                            "Moov atom seek detected: boosting priority for pieces {}-{}",
                            piece_idx_int, total_pieces - 1
                        );
                        for (int i = piece_idx_int; i < total_pieces; ++i) {
                            h.piece_priority(lt::piece_index_t(i), lt::top_priority);
                            // Stagger deadlines so libtorrent gets them in order
                            h.set_piece_deadline(lt::piece_index_t(i),
                                                 (i - piece_idx_int) * 200);
                        }
                    } else {
                        // Normal sequential piece: prioritize this piece + next 3
                        h.set_piece_deadline(piece_idx, 0, lt::torrent_handle::alert_when_available);
                        h.piece_priority(piece_idx, lt::top_priority);
                        for (int i = 1; i <= 3; ++i) {
                            lt::piece_index_t next_p(piece_idx_int + i);
                            if (next_p < lt::piece_index_t(total_pieces)) {
                                h.piece_priority(next_p, lt::top_priority);
                                h.set_piece_deadline(next_p, i * 1000);
                            }
                        }
                    }

                    // Wait longer for moov seeks — they require an out-of-order download
                    // from a peer connection that's currently serving sequential pieces.
                    // 4000ms gives libtorrent enough time to pivot to the new priority.
                    // Normal sequential pieces are available nearly instantly (500ms).
                    const int max_wait_ms = is_moov_seek ? 4000 : 500;
                    int waited = 0;
                    while (!h.have_piece(piece_idx) && waited < max_wait_ms) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        waited += 100;
                    }

                    if (!h.have_piece(piece_idx)) {
                        spdlog::warn(
                            "Timeout waiting for piece {} to download (moov_seek={}).",
                            piece_idx_int, is_moov_seek
                        );
                        return false;
                    }
                    return true;
                }
                return false;
            }
        }
        return false; // Torrent not found in session
    }

} // namespace media::core