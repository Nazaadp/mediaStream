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

#include <algorithm>
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

    // Season packs: a valid requested index wins; -1 (or out-of-range) falls
    // back to the largest file, which is the correct answer for single-file
    // torrents and the legacy behavior for everything else.
    static int resolveFileIndex(const lt::file_storage& finfo, int requested) {
        if (requested >= 0 && requested < finfo.num_files()) return requested;
        return findLargestFileIndex(finfo);
    }

    static std::string mimeTypeForPath(const std::filesystem::path& p) {
        const std::string ext = p.extension().string();
        if (ext == ".mkv")  return "video/x-matroska";
        if (ext == ".avi")  return "video/x-msvideo";
        if (ext == ".webm") return "video/webm";
        return "video/mp4";
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

        // Prefer encryption, fall back to plaintext for peers without RC4 support.
        // pe_forced cuts the peer pool drastically — many public-swarm peers
        // negotiate plaintext only, so pe_enabled keeps connectivity high while
        // still upgrading every link that supports it.
        p.set_int(lt::settings_pack::out_enc_policy, lt::settings_pack::pe_enabled);
        p.set_int(lt::settings_pack::in_enc_policy, lt::settings_pack::pe_enabled);
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
    void TorrentEngine::addMagnet(const std::string& magnet_uri, int file_index) {
        if (magnet_uri.empty() || magnet_uri.find("magnet:?") == std::string::npos) {
            spdlog::error("Security Alert: Invalid magnet link provided.");
            throw std::invalid_argument("Invalid magnet URI format");
        }

        try {
            lt::add_torrent_params params = lt::parse_magnet_uri(magnet_uri);

            std::string incoming_hash = to_hex_string(params.info_hashes.get_best());

            // Record the wanted file BEFORE the exists-check: for a season
            // pack, "add episode 2" arrives as the same magnet that is already
            // in the session, and the only effect must be enabling file 2.
            if (file_index >= 0) {
                std::lock_guard<std::mutex> lock(m_files_mutex);
                auto& wanted = m_wanted_files[incoming_hash];
                if (wanted.insert(file_index).second) {
                    // Wanted-set changed → priorities must be re-applied.
                    m_priorities_applied.erase(incoming_hash);
                }
            }

            // Check if it already exists
            std::vector<lt::torrent_handle> handles = m_session.get_torrents();
            for (const auto& h : handles) {
                if (h.is_valid() && to_hex_string(h.info_hash()) == incoming_hash) {
                    spdlog::info(
                        "Torrent already in session: {} (file_index {} enabled)",
                        incoming_hash, file_index);
                    // Metadata may already be present — apply immediately so
                    // the new episode starts downloading without waiting for
                    // the proactive loop's next tick.
                    applyFilePriorities(h, incoming_hash);
                    return;
                }
            }

            params.save_path = m_download_dir.string();

            // Streaming Optimization: Sequential Download
            params.flags |= lt::torrent_flags::sequential_download;

            m_session.async_add_torrent(params);
            spdlog::info("Magnet added to queue: {} (file_index {})", params.name, file_index);

        } catch (const std::exception& e) {
            spdlog::error("Failed to parse magnet URI: {}", e.what());
            throw;
        }
    }

    // --- APPLY PER-FILE PRIORITIES ---
    // Torrents with an explicit wanted-set download ONLY those files: with
    // sequential_download and all-default priorities, a season pack would
    // download episode 1 first no matter which episode was requested.
    // Priorities can only be applied once metadata is available, so this is
    // called both from addMagnet (dedup path) and every second from the
    // proactive loop until it sticks.
    void TorrentEngine::applyFilePriorities(const lt::torrent_handle& h, const std::string& hash) {
        if (!h.is_valid() || !h.torrent_file()) return;

        std::set<int> wanted;
        {
            std::lock_guard<std::mutex> lock(m_files_mutex);
            auto it = m_wanted_files.find(hash);
            if (it == m_wanted_files.end() || it->second.empty()) return; // legacy whole-torrent mode
            if (m_priorities_applied.count(hash)) return; // current set already applied
            wanted = it->second;
        }

        const auto& finfo = h.torrent_file()->files();
        const int n = finfo.num_files();
        std::vector<lt::download_priority_t> prios(static_cast<size_t>(n), lt::dont_download);
        int enabled = 0;
        for (int idx : wanted) {
            if (idx >= 0 && idx < n) {
                prios[static_cast<size_t>(idx)] = lt::default_priority;
                ++enabled;
            }
        }
        if (enabled == 0) {
            // Only invalid indices (bad fileIdx from the source) — keep the
            // default all-files behavior instead of downloading nothing.
            spdlog::warn("applyFilePriorities: no valid indices for {} — leaving defaults", hash);
            std::lock_guard<std::mutex> lock(m_files_mutex);
            m_priorities_applied.insert(hash);
            return;
        }

        h.prioritize_files(prios);
        {
            std::lock_guard<std::mutex> lock(m_files_mutex);
            m_priorities_applied.insert(hash);
        }
        spdlog::info("File priorities applied for {}: {}/{} files wanted", hash, enabled, n);
    }

    // --- REMOVE TORRENT ---
    bool TorrentEngine::removeTorrent(const std::string& info_hash_str, int file_index) {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();

        for (auto& h : handles) {
            if (!h.is_valid()) continue;

            std::string current_hash = to_hex_string(h.info_hash());

            if (current_hash == info_hash_str) {
                // Per-file removal: when other files of this torrent are still
                // wanted (season pack with several episodes on the go), only
                // stop downloading this one. The already-downloaded bytes of
                // the removed file stay on disk; full cleanup happens when the
                // last file is removed and the torrent itself is dropped.
                if (file_index >= 0) {
                    bool partial = false;
                    {
                        std::lock_guard<std::mutex> lock(m_files_mutex);
                        auto it = m_wanted_files.find(info_hash_str);
                        if (it != m_wanted_files.end() &&
                            it->second.count(file_index) &&
                            it->second.size() > 1) {
                            it->second.erase(file_index);
                            m_priorities_applied.erase(info_hash_str);
                            m_stream_boosted.erase(info_hash_str + ":" + std::to_string(file_index));
                            partial = true;
                        }
                    }
                    if (partial) {
                        if (h.torrent_file() &&
                            file_index < h.torrent_file()->files().num_files()) {
                            h.file_priority(lt::file_index_t(file_index), lt::dont_download);
                        }
                        {
                            std::lock_guard<std::mutex> lock(m_moov_mutex);
                            m_moov_boosted.erase(info_hash_str + ":" + std::to_string(file_index));
                        }
                        spdlog::info("File {} disabled on torrent {} (other files still wanted)",
                                     file_index, info_hash_str);
                        return true;
                    }
                }

                m_session.remove_torrent(h, lt::session::delete_files);
                {
                    std::lock_guard<std::mutex> lock(m_files_mutex);
                    m_wanted_files.erase(info_hash_str);
                    m_priorities_applied.erase(info_hash_str);
                    for (auto it = m_stream_boosted.begin(); it != m_stream_boosted.end(); ) {
                        if (it->rfind(info_hash_str, 0) == 0) it = m_stream_boosted.erase(it);
                        else ++it;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(m_moov_mutex);
                    for (auto it = m_moov_boosted.begin(); it != m_moov_boosted.end(); ) {
                        if (it->rfind(info_hash_str, 0) == 0) it = m_moov_boosted.erase(it);
                        else ++it;
                    }
                    // Clean up per-piece deadline tracking keys for this hash
                    for (auto it = m_moov_deadline_set.begin(); it != m_moov_deadline_set.end(); ) {
                        if (it->substr(0, info_hash_str.size()) == info_hash_str)
                            it = m_moov_deadline_set.erase(it);
                        else
                            ++it;
                    }
                }
                spdlog::info("Torrent removed with files: {}", info_hash_str);
                return true;
            }
        }
        // Not in the current session — files may still exist on disk from a
        // previous run; the caller decides whether to deleteDownloadedData().
        return false;
    }

    // --- DELETE DOWNLOADED DATA (out-of-session fallback) ---
    bool TorrentEngine::deleteDownloadedData(const std::string& file_path) {
        namespace fs = std::filesystem;
        if (file_path.empty()) return false;

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(m_download_dir, ec);
        if (ec) return false;
        const fs::path target = fs::weakly_canonical(fs::path(file_path), ec);
        if (ec) return false;

        // Safety: only ever delete entries strictly inside the download dir.
        const fs::path rel = target.lexically_relative(root);
        if (rel.empty() || rel == "." || *rel.begin() == "..") {
            spdlog::warn("deleteDownloadedData: refusing path outside download dir");
            return false;
        }

        // Remove the torrent's whole top-level entry (folder or single file),
        // not just the largest file — extras/subs would otherwise linger.
        const fs::path top = root / *rel.begin();
        const auto removed = fs::remove_all(top, ec);
        if (ec) {
            spdlog::error("deleteDownloadedData: remove failed: {}", ec.message());
            return false;
        }
        if (removed > 0) {
            spdlog::info("Deleted downloaded data ({} entries): {}", removed, top.string());
        }
        return removed > 0;
    }

    // --- PROACTIVE MOOV ATOM BOOST ---
    // Called every second by the WS broadcaster alongside getSessionStatus().
    // First applies any pending per-file priorities (metadata arrives async).
    // Then, as soon as a torrent has metadata and enters Downloading state,
    // boosts the last 5% of pieces OF EACH TARGET FILE to top priority AND
    // sets piece deadlines. Priority alone is insufficient — libtorrent will
    // not fetch a piece out-of-order from the current sequential position
    // unless a deadline is set. Deadlines convert a "preferred" piece into a
    // time-critical piece that libtorrent actively requests from peers
    // regardless of sequential order.
    //
    // File-relative, not torrent-relative: for a season pack, the moov atom
    // of episode 2 lives at the end of FILE 2, nowhere near the end of the
    // torrent — boosting the torrent's tail would fetch the last episode's
    // data instead.
    void TorrentEngine::proactivelyBoostEndPieces() {
        auto handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;
            if (!h.torrent_file()) continue; // Metadata not yet available

            std::string hash = to_hex_string(h.info_hash());

            // Pending priorities first — a freshly-added pack episode must be
            // enabled before any boosting math makes sense.
            applyFilePriorities(h, hash);

            auto ts = h.status();
            if (ts.state != lt::torrent_status::downloading) continue;

            const auto& finfo = h.torrent_file()->files();

            // Target files: the wanted-set for packs, else the largest file.
            std::set<int> targets;
            {
                std::lock_guard<std::mutex> lock(m_files_mutex);
                auto it = m_wanted_files.find(hash);
                if (it != m_wanted_files.end()) targets = it->second;
            }
            if (targets.empty()) {
                const int li = findLargestFileIndex(finfo);
                if (li < 0) continue;
                targets.insert(li);
            }

            const int piece_len = h.torrent_file()->piece_length();
            const int total_pieces = h.torrent_file()->num_pieces();
            if (piece_len <= 0 || total_pieces <= 0) continue;

            for (int idx : targets) {
                if (idx < 0 || idx >= finfo.num_files()) continue;

                const std::string key = hash + ":" + std::to_string(idx);
                {
                    std::lock_guard<std::mutex> lock(m_moov_mutex);
                    if (m_moov_boosted.count(key)) continue; // Already boosted
                }

                const int64_t f_off  = finfo.file_offset(lt::file_index_t(idx));
                const int64_t f_size = finfo.file_size(lt::file_index_t(idx));
                if (f_size <= 0) continue;

                int first_piece = static_cast<int>(f_off / piece_len);
                int last_piece  = static_cast<int>((f_off + f_size - 1) / piece_len);
                if (last_piece >= total_pieces) last_piece = total_pieces - 1;

                // Boost the last 5% of the FILE's pieces. For a 2GB file with
                // 512KB pieces (~4000 pieces), this is ~200 pieces = 100MB —
                // large enough to cover any moov atom or MKV cue table.
                const int span = last_piece - first_piece;
                const int boost_from = first_piece + span * 95 / 100;
                for (int i = boost_from; i <= last_piece; ++i) {
                    h.piece_priority(lt::piece_index_t(i), lt::top_priority);
                    // Stagger deadlines: first piece = 0ms, next = 500ms, etc.
                    // This tells libtorrent: «fetch these pieces out-of-order, urgently».
                    h.set_piece_deadline(lt::piece_index_t(i), (i - boost_from) * 500);
                }
                spdlog::info(
                    "Proactive moov boost: pieces {}-{} (file {}) queued for hash {}",
                    boost_from, last_piece, idx, hash
                );
                {
                    std::lock_guard<std::mutex> lock(m_moov_mutex);
                    m_moov_boosted.insert(key);
                }
            }
        }
    }

    // --- GET STATUS ---
    std::vector<TorrentStatus> TorrentEngine::getSessionStatus() const {
        std::vector<TorrentStatus> statuses;
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();

        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            lt::torrent_status ts = h.status();
            TorrentStatus base;

            base.info_hash = to_hex_string(h.info_hash());
            base.name = ts.name;
            base.progress = ts.progress; // Raw float 0.0 - 1.0

            // Map state
            switch(ts.state) {
                case lt::torrent_status::checking_files: base.state = "Checking"; break;
                case lt::torrent_status::downloading_metadata: base.state = "Fetching Metadata"; break;
                case lt::torrent_status::downloading: base.state = "Downloading"; break;
                case lt::torrent_status::finished: base.state = "Finished"; break;
                case lt::torrent_status::seeding: base.state = "Seeding"; break;
                default: base.state = "Queued"; break;
            }

            base.download_rate = ts.download_payload_rate;

            // Without metadata there is nothing file-level to report yet.
            if (!h.torrent_file()) {
                statuses.push_back(base);
                continue;
            }

            const auto& finfo = h.torrent_file()->files();

            // Wanted-set torrents (season packs): one entry PER requested
            // file, each with per-file progress/name/size, so the client can
            // show episode 1 as complete while episode 2 is still at 40%.
            std::set<int> wanted;
            {
                std::lock_guard<std::mutex> lock(m_files_mutex);
                auto it = m_wanted_files.find(base.info_hash);
                if (it != m_wanted_files.end()) wanted = it->second;
            }

            if (!wanted.empty()) {
                std::vector<std::int64_t> fp;
                h.file_progress(fp);
                for (int idx : wanted) {
                    if (idx < 0 || idx >= finfo.num_files()) continue;
                    TorrentStatus s = base;
                    s.file_index = idx;

                    std::filesystem::path fsp =
                        m_download_dir / finfo.file_path(lt::file_index_t(idx));
                    s.filename   = fsp.filename().string();
                    s.size_bytes = finfo.file_size(lt::file_index_t(idx));
                    s.mime_type  = mimeTypeForPath(fsp);

                    if (s.size_bytes > 0 && static_cast<size_t>(idx) < fp.size()) {
                        const double done = static_cast<double>(fp[static_cast<size_t>(idx)]);
                        s.progress = static_cast<float>(
                            std::min(1.0, done / static_cast<double>(s.size_bytes)));
                        // A fully-downloaded episode reads as Finished even
                        // while sibling files keep the torrent Downloading.
                        if (s.progress >= 0.999f) s.state = "Finished";
                    }
                    statuses.push_back(s);
                }
                continue;
            }

            // Legacy single-entry path: metadata of the largest file.
            int largest_index = findLargestFileIndex(finfo);
            if (largest_index != -1) {
                std::filesystem::path fsp =
                    m_download_dir / finfo.file_path(lt::file_index_t(largest_index));

                base.filename  = fsp.filename().string();
                base.size_bytes = finfo.file_size(lt::file_index_t(largest_index));
                base.mime_type  = mimeTypeForPath(fsp);
            }

            statuses.push_back(base);
        }
        return statuses;
    }

    // --- GET FILE PATH (per-file aware) ---
    std::optional<std::string> TorrentEngine::getFilePath(const std::string& info_hash_str, int file_index) const {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            if (to_hex_string(h.info_hash()) == info_hash_str) {
                if (!h.torrent_file()) return std::nullopt; // Metadata not yet downloaded

                const auto& finfo = h.torrent_file()->files();
                const int idx = resolveFileIndex(finfo, file_index);

                if (idx != -1) {
                    std::filesystem::path full_path = m_download_dir;
                    full_path /= finfo.file_path(lt::file_index_t(idx));
                    return full_path.string();
                }
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> TorrentEngine::getLargestFilePath(const std::string& info_hash_str) const {
        return getFilePath(info_hash_str, -1);
    }

    // --- WAIT FOR PIECE ---
    // Returns true if the requested piece became available within the timeout.
    // Returns false if the torrent was not found, metadata not ready, or timeout elapsed.
    // file_offset is relative to the resolved target file.
    bool TorrentEngine::waitForPiece(const std::string& info_hash_str, uint64_t file_offset, int file_index) {
        std::vector<lt::torrent_handle> handles = m_session.get_torrents();
        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            if (to_hex_string(h.info_hash()) == info_hash_str) {
                if (!h.torrent_file()) return false; // Metadata not yet downloaded

                const auto& finfo = h.torrent_file()->files();
                const int idx = resolveFileIndex(finfo, file_index);

                if (idx != -1) {
                    const int64_t f_off  = finfo.file_offset(lt::file_index_t(idx));
                    const int64_t f_size = finfo.file_size(lt::file_index_t(idx));
                    if (f_size <= 0) return false;

                    // The file being actively streamed outranks background
                    // prefetches of sibling episodes: bump it to top priority
                    // once, and demote any previously-boosted sibling.
                    {
                        const std::string skey = info_hash_str + ":" + std::to_string(idx);
                        bool bump = false;
                        {
                            std::lock_guard<std::mutex> lock(m_files_mutex);
                            if (!m_stream_boosted.count(skey)) {
                                for (auto it = m_stream_boosted.begin(); it != m_stream_boosted.end(); ) {
                                    if (it->rfind(info_hash_str, 0) == 0) it = m_stream_boosted.erase(it);
                                    else ++it;
                                }
                                m_stream_boosted.insert(skey);
                                bump = true;
                            }
                        }
                        if (bump) {
                            std::set<int> wanted;
                            {
                                std::lock_guard<std::mutex> lock(m_files_mutex);
                                auto wit = m_wanted_files.find(info_hash_str);
                                if (wit != m_wanted_files.end()) wanted = wit->second;
                            }
                            for (int w : wanted) {
                                if (w < 0 || w >= finfo.num_files()) continue;
                                h.file_priority(lt::file_index_t(w),
                                                w == idx ? lt::top_priority : lt::default_priority);
                            }
                        }
                    }

                    // Calculate the absolute byte offset within the torrent
                    int64_t torrent_offset = f_off + static_cast<int64_t>(file_offset);
                    // Calculate piece index
                    int piece_length = h.torrent_file()->piece_length();
                    if (piece_length <= 0) return false;

                    lt::piece_index_t piece_idx(torrent_offset / piece_length);

                    // If piece index is beyond total pieces, return
                    const int total_pieces = h.torrent_file()->num_pieces();
                    if (piece_idx >= lt::piece_index_t(total_pieces)) return false;

                    // Fast path: if already downloaded, return immediately
                    if (h.have_piece(piece_idx)) return true;

                    // The FILE's own piece range — all boost math below is
                    // file-relative. For a season pack, "the end of the video"
                    // is the end of this file, not the end of the torrent.
                    const int first_piece = static_cast<int>(f_off / piece_length);
                    int last_piece = static_cast<int>((f_off + f_size - 1) / piece_length);
                    if (last_piece >= total_pieces) last_piece = total_pieces - 1;

                    // --- MOOV ATOM DETECTION ---
                    // The player seeks to the end of the file to find the MP4 moov atom
                    // (or MKV segment index). This seek lands on a piece far beyond the
                    // current sequential download position. We detect this by checking
                    // if the requested piece is in the last 15% of the FILE.
                    const int piece_idx_int = static_cast<int>(piece_idx);
                    const int span = last_piece - first_piece;
                    const bool is_moov_seek = piece_idx_int > (first_piece + span * 85 / 100);

                    if (is_moov_seek) {
                        // Guard: only set deadlines once per moov-seek piece to avoid
                        // thrashing libtorrent's scheduler on every 503 retry from the
                        // frontend. Re-setting deadlines repeatedly resets the countdown,
                        // which can delay delivery rather than accelerate it.
                        std::string moov_key = info_hash_str + ":" + std::to_string(piece_idx_int);
                        bool already_scheduled;
                        {
                            std::lock_guard<std::mutex> lock(m_moov_mutex);
                            already_scheduled = m_moov_deadline_set.count(moov_key) != 0;
                            if (!already_scheduled) m_moov_deadline_set.insert(moov_key);
                        }
                        if (!already_scheduled) {
                            spdlog::info(
                                "Moov atom seek detected: boosting priority for pieces {}-{} (file {})",
                                piece_idx_int, last_piece, idx
                            );
                            for (int i = piece_idx_int; i <= last_piece; ++i) {
                                h.piece_priority(lt::piece_index_t(i), lt::top_priority);
                                // Stagger deadlines so libtorrent fetches in sequential order
                                // starting immediately (0ms for the first critical piece).
                                h.set_piece_deadline(lt::piece_index_t(i),
                                                     (i - piece_idx_int) * 200);
                            }
                        } else {
                            spdlog::debug(
                                "Moov seek for piece {} already scheduled, waiting...",
                                piece_idx_int
                            );
                        }
                    } else {
                        // Normal sequential piece: prioritize this piece + next 3
                        // (clamped to the file — the next piece past last_piece
                        // belongs to a sibling episode we may not even want).
                        h.set_piece_deadline(piece_idx, 0, lt::torrent_handle::alert_when_available);
                        h.piece_priority(piece_idx, lt::top_priority);
                        for (int i = 1; i <= 3; ++i) {
                            const int next_i = piece_idx_int + i;
                            if (next_i <= last_piece) {
                                lt::piece_index_t next_p(next_i);
                                h.piece_priority(next_p, lt::top_priority);
                                h.set_piece_deadline(next_p, i * 1000);
                            }
                        }
                    }

                    // Moov seeks require 15s: the proactive boost sets deadlines but
                    // libtorrent still needs to negotiate an out-of-order request with a
                    // peer and receive the piece data. In testing, delivery took up to
                    // 56s total (14 × 4s retries) — 15s per attempt with frontend retries
                    // covers this without holding the oatpp thread pool hostage too long.
                    // Sequential pieces: 8s timeout instead of 500ms.
                    // At low download progress (5-15%), libtorrent may not yet have
                    // buffered the next few sequential pieces — a 503 returned mid-stream
                    // triggers FFmpegDemuxer: PIPELINE_ERROR_READ (MediaError code 2) in
                    // WebView2, which the frontend retry loop cannot safely handle.
                    // Waiting up to 8s server-side prevents the mid-stream 503 entirely.
                    const int max_wait_ms = is_moov_seek ? 15000 : 8000;
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
