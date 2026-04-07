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
            
            std::string current_hash = to_hex_string(h.info_hash());
            
            if (current_hash == info_hash_str) {
                m_session.remove_torrent(h);
                m_moov_boosted.erase(info_hash_str); // Allow re-add to re-boost
                spdlog::info("Torrent removed: {}", info_hash_str);
                return;
            }
        }
        throw std::runtime_error("Torrent not found");
    }

    // --- PROACTIVE MOOV ATOM BOOST ---
    // Called every second by the WS broadcaster alongside getSessionStatus().
    // As soon as a torrent has metadata and enters Downloading state, we boost
    // the last 5% of pieces to top priority AND set piece deadlines.
    // Priority alone is insufficient — libtorrent will not fetch a piece
    // out-of-order from the current sequential position unless a deadline is set.
    // Deadlines convert a "preferred" piece into a time-critical piece that
    // libtorrent actively requests from peers regardless of sequential order.
    void TorrentEngine::proactivelyBoostEndPieces() {
        auto handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;
            if (!h.torrent_file()) continue; // Metadata not yet available

            auto ts = h.status();
            if (ts.state != lt::torrent_status::downloading) continue;

            std::string hash = to_hex_string(h.info_hash());
            if (m_moov_boosted.count(hash)) continue; // Already boosted

            const int total_pieces = h.torrent_file()->num_pieces();
            // Boost the last 5% of pieces. For a 2GB torrent with 512KB pieces
            // (~4000 pieces), this is the last 200 pieces = 100MB — large enough
            // to cover any moov atom or MKV cue table.
            const int boost_from = total_pieces * 95 / 100;
            for (int i = boost_from; i < total_pieces; ++i) {
                h.piece_priority(lt::piece_index_t(i), lt::top_priority);
                // Stagger deadlines: first piece = 0ms, next = 100ms, etc.
                // Reduced from 500ms to 100ms to fetch moov pieces 5x faster.
                // For 60 pieces (5% of 1200), this is 6 seconds total vs 30 seconds.
                h.set_piece_deadline(lt::piece_index_t(i), (i - boost_from) * 100);
            }
            spdlog::info(
                "Proactive moov boost: pieces {}-{} queued (with deadlines) for hash {}",
                boost_from, total_pieces - 1, hash
            );
            m_moov_boosted.insert(hash);
        }
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
                    const int total_pieces = h.torrent_file()->num_pieces();
                    const int piece_idx_int = static_cast<int>(piece_idx);
                    const bool is_moov_seek = piece_idx_int > (total_pieces * 85 / 100);

                    if (is_moov_seek) {
                        // Guard: only set deadlines once per moov-seek piece to avoid
                        // thrashing libtorrent's scheduler on every 503 retry from the
                        // frontend. Re-setting deadlines repeatedly resets the countdown,
                        // which can delay delivery rather than accelerate it.
                        std::string moov_key = info_hash_str + ":" + std::to_string(piece_idx_int);
                        if (!m_moov_deadline_set.count(moov_key)) {
                            spdlog::info(
                                "Moov atom seek detected: boosting priority for pieces {}-{}",
                                piece_idx_int, total_pieces - 1
                            );
                            for (int i = piece_idx_int; i < total_pieces; ++i) {
                                h.piece_priority(lt::piece_index_t(i), lt::top_priority);
                                // Stagger deadlines so libtorrent fetches in sequential order
                                // starting immediately (0ms for the first critical piece).
                                // Reduced from 200ms to 100ms to match proactive boost timing.
                                h.set_piece_deadline(lt::piece_index_t(i),
                                                     (i - piece_idx_int) * 100);
                            }
                            m_moov_deadline_set.insert(moov_key);
                        } else {
                            spdlog::debug(
                                "Moov seek for piece {} already scheduled, waiting...",
                                piece_idx_int
                            );
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

                    // Moov seeks require 30s: the proactive boost sets deadlines staggered
                    // at 100ms intervals for ~60 pieces (5% of torrent) = 6 seconds of
                    // staggered requests. However, libtorrent still needs to negotiate
                    // out-of-order requests with peers and receive the data. In practice,
                    // with slow peers or high latency, this can take 20-30 seconds.
                    // Increased from 15s to 30s to reduce 503 retries on the frontend.
                    // Normal sequential pieces arrive in <500ms (already buffered).
                    const int max_wait_ms = is_moov_seek ? 30000 : 500;
                    int waited = 0;
                    while (!h.have_piece(piece_idx) && waited < max_wait_ms) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        waited += 100;
                    }

                    if (!h.have_piece(piece_idx)) {
                        spdlog::warn(
                            "Timeout waiting for piece {} to download (moov_seek={}, waited={}ms).",
                            piece_idx_int, is_moov_seek, waited
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