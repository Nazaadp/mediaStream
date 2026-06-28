#pragma once

// TorrentScorer.hpp
// ─────────────────────────────────────────────────────────────────────────────
// Advanced metadata extraction + weighted quality scoring for TorrentQuality.
//
// Usage (after setting hash / seeders / size_bytes / title on the struct):
//
//   // Prefer behaviorHints.filename for Torrentio; fall back to tq.title
//   TorrentScorer::enrich(tq, parse_name);
//   tq.score = TorrentScorer::score(tq);
//
// Then sort:
//   std::sort(v.begin(), v.end(), TorrentScorer::better);
//
// ─────────────────────────────────────────────────────────────────────────────

#include "mediastream/services/ContentDiscovery.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <regex>
#include <string>

namespace media::services::TorrentScorer {

// ─────────────────────────────────────────────────────────────────────────────
// § 1  String normalisation
//
// Replaces dots, underscores, and newlines with spaces; collapses runs; pads
// both ends with a space so every token is surrounded by spaces — enabling
// simple \bFOO\b matching without look-arounds.
// Does NOT lowercase: all regexes below use std::regex::icase.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string normalize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    out += ' ';
    bool prev_space = true;
    for (unsigned char c : raw) {
        if (c == '.' || c == '_' || c == '\n' || c == '\r') {
            if (!prev_space) { out += ' '; prev_space = true; }
        } else {
            out += static_cast<char>(c);
            prev_space = (c == ' ');
        }
    }
    if (out.empty() || out.back() != ' ') out += ' ';
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 2  Resolution
//
// Matches standard pixel-height tokens: 480p 576p 720p 1080p 2160p 4320p.
// The character class before the digits prevents matching codec strings like
// "x265" (which has 'x' immediately before the digits, not a separator).
// Falls back to the "4K" / "4k" label used by some Torrentio name fields.
// ─────────────────────────────────────────────────────────────────────────────
inline int parseResolution(const std::string& normalized) {
    // Group order matches return value array below.
    static const std::regex re(
        R"((?:^|[\s\[\(\-\,])(?:(4320)|(2160)|(1080)|(720)|(576)|(480))p(?=[\s\]\)\.\,_]|$))",
        std::regex::icase | std::regex::optimize
    );
    static const std::regex re_4k(R"(\b4[Kk]\b)", std::regex::optimize);

    std::smatch m;
    if (std::regex_search(normalized, m, re)) {
        constexpr int kVals[] = {4320, 2160, 1080, 720, 576, 480};
        for (int i = 0; i < 6; ++i)
            if (m[i + 1].matched) return kVals[i];
    }
    if (std::regex_search(normalized, re_4k)) return 2160;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 3  Video codec
//
// Priority order: AV1 > HEVC family > x264/AVC family > XviD/DivX.
// All aliases compiled into one regex per tier for speed.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string parseCodec(const std::string& normalized) {
    static const std::regex re_av1(
        R"(\bAV1\b)",
        std::regex::icase | std::regex::optimize);

    static const std::regex re_hevc(
        R"(\b(?:HEVC|H\.?265|H265|x265)\b)",
        std::regex::icase | std::regex::optimize);

    static const std::regex re_x264(
        R"(\b(?:x264|H\.?264|H264|AVC)\b)",
        std::regex::icase | std::regex::optimize);

    static const std::regex re_xvid(
        R"(\b(?:XviD|Xvid|DivX)\b)",
        std::regex::icase | std::regex::optimize);

    if (std::regex_search(normalized, re_av1))  return "AV1";
    if (std::regex_search(normalized, re_hevc)) return "HEVC";
    if (std::regex_search(normalized, re_x264)) return "x264";
    if (std::regex_search(normalized, re_xvid)) return "XviD";
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// § 4  Visual enhancements (HDR / Dolby Vision)
//
// Detects HDR10+ / HDR10 / HDR and DV / Dolby Vision independently.
// is_hdr is set true whenever either HDR10 or HDR is present.
// ─────────────────────────────────────────────────────────────────────────────
inline void parseHDR(const std::string& normalized,
                     bool& is_hdr, bool& is_hdr10, bool& is_dv) {
    // Dolby Vision aliases — must come before plain HDR check
    static const std::regex re_dv(
        R"(\b(?:DV|Dolby[\.\s]?Vision)\b)",
        std::regex::icase | std::regex::optimize);

    // HDR10+ / HDR10 — must come before plain HDR check
    static const std::regex re_hdr10(
        R"(\bHDR10(?:\+|Plus)?\b)",
        std::regex::icase | std::regex::optimize);

    // Plain HDR — matches "HDR" not already covered above
    // Negative lookahead prevents matching "HDR10" a second time.
    static const std::regex re_hdr(
        R"(\bHDR(?!10)\b)",
        std::regex::icase | std::regex::optimize);

    is_dv    = std::regex_search(normalized, re_dv);
    is_hdr10 = std::regex_search(normalized, re_hdr10);
    is_hdr   = is_hdr10 || std::regex_search(normalized, re_hdr);
}

// ─────────────────────────────────────────────────────────────────────────────
// § 5  Release type
//
// Detects REMUX, BluRay family, and WEB-DL family.
// is_remux and is_bluray are orthogonal (a BluRay REMUX sets both).
// WEBRip is intentionally NOT set as is_webdl — it scores lower.
// ─────────────────────────────────────────────────────────────────────────────
inline void parseReleaseType(const std::string& normalized,
                              bool& is_remux, bool& is_bluray, bool& is_webdl) {
    static const std::regex re_remux(
        R"(\b(?:REMUX|BDRemux)\b)",
        std::regex::icase | std::regex::optimize);

    static const std::regex re_bluray(
        R"(\b(?:BluRay|BLU[\-\s]?RAY|Blu[\-\s]ray|BDRip|BDMux|UHD[\.\s]?Blu[\-\s]?[Rr]ay)\b)",
        std::regex::icase | std::regex::optimize);

    static const std::regex re_webdl(
        R"(\b(?:WEB[\-\.]?DL|WEBDL|WEBMux)\b)",
        std::regex::icase | std::regex::optimize);

    is_remux  = std::regex_search(normalized, re_remux);
    is_bluray = std::regex_search(normalized, re_bluray);
    is_webdl  = std::regex_search(normalized, re_webdl);
}

// ─────────────────────────────────────────────────────────────────────────────
// § 6  Garbage detection (CAM / TS / TeleSync etc.)
//
// Requires whole-word boundaries to avoid false positives in tracker names.
// The standalone "TS" alternative uses explicit surrounding separators because
// "TS" can legitimately appear inside source names (e.g., "AMZN.WEBMux.TS-EN").
// ─────────────────────────────────────────────────────────────────────────────
inline bool isGarbage(const std::string& normalized) {
    static const std::regex re_cam(
        R"(\b(?:CAM|CAMRIP|HDCAM|HDTS|TeleSync|TELESYNC|WORKPRINT|WP|SCREENER|SCR)\b)"
        R"(|(?:^|[\s\[\(\-\.])TS(?:[\s\]\)\-\.]|$))",
        std::regex::icase | std::regex::optimize);

    return std::regex_search(normalized, re_cam);
}

// ─────────────────────────────────────────────────────────────────────────────
// § 7  YTS structured-data fast path
//
// YTS already provides quality/video_codec/type as structured JSON fields, so
// we map them directly rather than re-parsing strings.
// Call this instead of enrich() for YTS-sourced torrents.
// ─────────────────────────────────────────────────────────────────────────────
inline void enrichFromYTS(TorrentQuality& tq,
                           const std::string& yts_quality,   // "720p", "1080p", "2160p"
                           const std::string& yts_codec,     // "x264", "x265"
                           const std::string& yts_type,      // "web", "bluray"
                           const std::string& yts_bit_depth) // "8", "10"
{
    // Resolution from quality string
    if      (yts_quality == "4320p") tq.resolution_p = 4320;
    else if (yts_quality == "2160p") tq.resolution_p = 2160;
    else if (yts_quality == "1080p") tq.resolution_p = 1080;
    else if (yts_quality == "720p")  tq.resolution_p = 720;
    else if (yts_quality == "480p")  tq.resolution_p = 480;

    // Codec
    if      (yts_codec == "x265") tq.codec = "HEVC";
    else if (yts_codec == "x264") tq.codec = "x264";
    else if (yts_codec == "AV1")  tq.codec = "AV1";
    else                           tq.codec = yts_codec; // passthrough

    // YTS never serves CAM/TS
    tq.is_cam = false;

    // HDR: YTS doesn't expose HDR in the API — infer from 10-bit 2160p
    if (tq.resolution_p == 2160 && yts_bit_depth == "10") {
        tq.is_hdr = true;
    }

    // Release type from YTS "type" field
    if (yts_type == "bluray") {
        tq.is_bluray = true;
    } else if (yts_type == "web") {
        tq.is_webdl = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// § 8  Generic enrichment (Torrentio / EZTV / Nyaa)
//
// parse_name: the richest name available for this torrent.
//   • Torrentio: prefer behaviorHints.filename; fall back to parseName(title)
//   • EZTV:      use the "filename" JSON field if present, else "title"
//   • Nyaa:      use the RSS <title> element
//
// Normalisation is applied internally; callers pass the raw string.
// ─────────────────────────────────────────────────────────────────────────────
inline void enrich(TorrentQuality& tq, const std::string& parse_name) {
    const std::string norm = normalize(parse_name);

    tq.resolution_p = parseResolution(norm);
    tq.codec        = parseCodec(norm);

    parseHDR(norm, tq.is_hdr, tq.is_hdr10, tq.is_dv);
    parseReleaseType(norm, tq.is_remux, tq.is_bluray, tq.is_webdl);

    tq.is_cam = isGarbage(norm);

    // Sync legacy "quality" string if not already set from structured data
    if (tq.quality.empty() || tq.quality == tq.title) {
        tq.quality = (tq.resolution_p > 0)
                     ? std::to_string(tq.resolution_p) + "p"
                     : "unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// § 9  Weighted quality score
//
// Score components and their ceiling:
//   Resolution        0 – 100
//   Codec             0 –  20
//   HDR / DV          0 –  20
//   Release type      0 –  25
//   Seeder log curve  0 –  50   (tie-breaker; log₂ scale)
//   ─────────────────────────────
//   Max                    215
//
// Garbage (CAM/TS) returns INT_MIN — caller should filter/drop.
//
// Seeder formula: min(50, log2(seeders + 1) × 7.2)
//   0 seeders   →  0
//   2 seeders   →  8
//   10 seeders  → 25
//   100 seeders → 48
//   500+ seeders→ 50 (saturates)
//
// This prevents a 480p CAM-rip with 5 000 seeders (score ≈ 65) from ranking
// above a 4K WEB-DL HDR with 50 seeders (score ≈ 171).
// ─────────────────────────────────────────────────────────────────────────────
inline int score(const TorrentQuality& t) {
    if (t.is_cam) return std::numeric_limits<int>::min(); // hard drop

    int s = 0;

    // ── Resolution (0-100) ────────────────────────────────────────────────────
    switch (t.resolution_p) {
        case 4320: s += 100; break;
        case 2160: s +=  80; break;
        case 1080: s +=  60; break;
        case  720: s +=  40; break;
        case  576: s +=  25; break;
        case  480: s +=  15; break;
        default:   s +=  35; break; // unknown: neutral; don't bury it
    }

    // ── Codec (0-20) ──────────────────────────────────────────────────────────
    if      (t.codec == "AV1")  s += 20;
    else if (t.codec == "HEVC") s += 16;
    else if (t.codec == "x264") s += 10;
    else if (t.codec == "XviD") s +=  3;
    // Unknown codec → +0: YTS sometimes omits codec but is still high quality

    // ── HDR / Visual Enhancement (0-20) ───────────────────────────────────────
    // Combinations rank above individual flags
    if      (t.is_dv  && t.is_hdr10) s += 20;
    else if (t.is_dv  && t.is_hdr)   s += 18;
    else if (t.is_hdr10)              s += 16;
    else if (t.is_hdr)                s += 12;
    else if (t.is_dv)                 s +=  8; // DV-only (profile 5, rare)

    // ── Release type (0-25) ───────────────────────────────────────────────────
    // REMUX and WEB-DL are not mutually exclusive (e.g. WEB-DL BluRay REMUX)
    if (t.is_remux)                         s += 25;
    else if (t.is_webdl && t.is_bluray)     s += 15;
    else if (t.is_webdl)                    s += 15;
    else if (t.is_bluray)                   s += 10;
    // WEBRip, UHDRip, etc. → +0

    // ── Seeder log curve (0-50) ───────────────────────────────────────────────
    if (t.seeders > 0) {
        const double log_s = std::log2(static_cast<double>(t.seeders) + 1.0) * 7.2;
        s += static_cast<int>(std::min(50.0, log_s));
    }

    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 10  Sort comparator
//
// Use with std::sort / std::stable_sort:
//   std::sort(results.begin(), results.end(), TorrentScorer::better);
// ─────────────────────────────────────────────────────────────────────────────
inline bool better(const TorrentQuality& a, const TorrentQuality& b) {
    return a.score > b.score;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 11  Episode number regexes
//
// buildNyaaEpisodePattern(ep)
// ────────────────────────────
// Handles absolute anime numbering (e.g. episode 105 in a long-running show)
// in addition to standard E## / "- ##" / [##] notation.
//
// Safety constraints:
//   • Must be preceded by a separator (space, dash, dot, bracket, underscore)
//     OR be at start-of-string — this prevents matching the '265' in 'x265'.
//   • Must NOT be followed by 'p'   — prevents matching '1080' in '1080p'.
//   • Must NOT be followed by 'bit' — prevents matching '10' in '10bit'.
//   • Must NOT be followed by another digit — prevents partial matches.
//   • Optional trailing version tag 'v\d+' (common on Nyaa: "05v2").
//
// For episode 5, matches:
//   "- 05 [1080p]"   "E05 "   "[05]"   "05v2 "   " 5 "
// Does NOT match:
//   "x265"  "1080p"  "10bit"  "5.1"  "50 "  (partial)
//
// buildEztvDailyPattern()
// ────────────────────────
// Matches YYYY.MM.DD or YYYY-MM-DD as a fallback when no S##E## is found,
// for daily shows (e.g. "The.Daily.Show.2024.03.15.1080p").
// ─────────────────────────────────────────────────────────────────────────────
inline std::string buildNyaaEpisodePattern(int episode) {
    assert(episode > 0);
    // The "0*" prefix allows matching "05" and "5" for the same episode.
    const std::string ep = std::to_string(episode);
    return
        // Alternative 1: separator + optional zeros + episode + optional v# + non-digit/non-p/non-bit
        "(?:^|[-\\s\\.\\[\\(_])0*" + ep + "(?:v\\d+)?(?!\\d|p(?=\\s|\\]|$)|bit)"
        // Alternative 2: E## or e## notation
        "|[Ee]0*" + ep + "(?:v\\d+)?(?!\\d|p(?=\\s|\\]|$)|bit)"
        // Alternative 3: explicit bracket notation [##] or [##v2]
        "|\\[0*" + ep + "(?:v\\d+)?\\]";
}

inline std::string buildEztvDailyPattern() {
    // Matches "2024.03.15" or "2024-03-15" (daily show air date as episode ID)
    return R"(\b\d{4}[.\-]\d{2}[.\-]\d{2}\b)";
}

// ─────────────────────────────────────────────────────────────────────────────
// § 12  Full pipeline helpers for ContentDiscoveryManager
//
// enrichAndScore(results)
//   Enriches every element and computes its score in place.
//   Removes CAM/TS entries.
//   Sorts by score descending.
//   Call this in place of the old manual std::sort lambda.
// ─────────────────────────────────────────────────────────────────────────────
inline void enrichAndScore(std::vector<TorrentQuality>& results,
                            bool is_yts_source = false)
{
    for (auto& t : results) {
        // For non-YTS: enrich from the torrent title/filename
        if (!is_yts_source) {
            // Use title as the parse target; Torrentio callers should have
            // already set tq.title to behaviorHints.filename when available.
            enrich(t, t.title);
        }
        t.score = score(t);
    }

    // Remove garbage entries (CAM/TS score = INT_MIN)
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [](const TorrentQuality& t){ return t.is_cam; }),
        results.end());

    std::sort(results.begin(), results.end(), better);
}

// scoreAndSort(results)
//   For pipelines where each source client has ALREADY enriched its own
//   torrents (e.g. YTS via enrichFromYTS for its structured codec/bit-depth,
//   others via enrich() on the release name). Computes the score in place,
//   drops CAM/TS entries, and sorts by score descending — WITHOUT re-enriching
//   (which would clobber the per-source metadata, notably YTS's codec/HDR).
inline void scoreAndSort(std::vector<TorrentQuality>& results) {
    for (auto& t : results) t.score = score(t);

    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [](const TorrentQuality& t){ return t.is_cam; }),
        results.end());

    std::sort(results.begin(), results.end(), better);
}

} // namespace media::services::TorrentScorer
