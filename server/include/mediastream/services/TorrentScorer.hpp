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
#include <utility>
#include <vector>

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
// § 13a  Structured name parsing — type + forward declaration
//
// The implementation is § 13 at the end of this file. § 8 enrich() calls
// parse(), so the complete type and the signature must already be visible here;
// an inline function may be declared before it is defined in the same TU.
// ─────────────────────────────────────────────────────────────────────────────
struct ParsedName {
    std::string title;           // clean human title; "" = extraction failed
    int         year{0};         // 0 = none found
    int         season{0};       // 0 = none
    int         season_end{0};   // >0 only for season ranges (S01-S05) ⇒ pack
    int         episode{0};      // 0 = none
    int         episode_end{0};  // >0 only for ranges (S01E01-E13, (01-24)) ⇒ pack
    int         resolution_p{0}; // height from a "WxH" or "NNNNp" token; 0 = none
    bool        is_daily{false};
    std::string air_date;        // "YYYY-MM-DD" when is_daily
    std::string release_group;   // "SubsPlease" (leading [..]) / "SPARKS" (trailing -..)
};

inline ParsedName parse(const std::string& raw_name);

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

    // Structured capture (§ 13). Single entry point: Torrentio, EZTV and Nyaa
    // all get title/year/season/episode/group for free, no client edits.
    const ParsedName p = parse(parse_name);
    tq.parsed_title  = p.title;
    tq.year          = p.year;
    tq.season        = p.season;
    tq.episode       = p.episode;
    tq.episode_end   = p.episode_end;
    tq.release_group = p.release_group;

    // "1920x1080" carries a resolution the § 2 "NNNNp" tokens miss. Fallback
    // only — never overrides a value parseResolution() already found.
    if (tq.resolution_p == 0) tq.resolution_p = p.resolution_p;

    // An explicit episode or season RANGE proves the torrent bundles more than
    // one episode. looksLikeSeasonPack() (§ 11b, run by the clients on the
    // TORRENT-level name) stays as the independent second opinion — OR, never
    // overwrite: the per-file name of a pack always looks single-episode.
    tq.is_pack = tq.is_pack ||
                 (p.episode > 0 && p.episode_end > p.episode) ||
                 (p.season  > 0 && p.season_end  > p.season);

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
// § 11b  Season-pack detection
//
// Decides from the TORRENT-level name (Torrentio title line 1, EZTV/Nyaa
// title) whether the torrent bundles multiple episodes. Order matters:
// explicit multi-episode signals win over single-episode markers, because a
// pack name like "S01E01-E13" contains both.
//
//   true:   "Breaking Bad S01-S05 Complete 1080p"     (keyword + range)
//           "[SubsPlease] Frieren (01-28) [Batch]"    (range + keyword)
//           "Show S01E01-E08 720p"                    (episode range)
//           "Show Season 2 1080p" / "Show S02 x265"   (season, no episode)
//   false:  "Show S01E02 1080p"                       (single episode)
//           "Show 1x02 HDTV"                          (single episode)
//           "[SubsPlease] Frieren - 05 (1080p)"       (anime single episode)
//           "Inception 2010 1080p BluRay"             (movie)
// ─────────────────────────────────────────────────────────────────────────────
inline bool looksLikeSeasonPack(const std::string& torrent_name) {
    const std::string n = normalize(torrent_name);

    // 1. Explicit multi-episode keywords
    static const std::regex re_kw(
        R"(\b(?:batch|complete|collection|integrale?|full\s?(?:season|series)|all\s?episodes|duology|trilogy)\b)",
        std::regex::icase | std::regex::optimize);
    // 2. Season ranges: "S01-S05", "S01-05", "Seasons 1-5"
    static const std::regex re_srange(
        R"(\bS\d{1,2}\s*-\s*S?\d{1,2}\b|\bseasons?\s+\d{1,2}\s*-\s*\d{1,2}\b)",
        std::regex::icase | std::regex::optimize);
    // 3. Episode ranges: "E01-E13", "(01-24)", "[01~24]"
    static const std::regex re_erange(
        R"(\bE\d{1,3}\s*[-~]\s*E?\d{1,3}\b|[\[\(]\s*\d{1,3}\s*[-~]\s*\d{1,3}\s*[\]\)])",
        std::regex::icase | std::regex::optimize);
    if (std::regex_search(n, re_kw) || std::regex_search(n, re_srange) ||
        std::regex_search(n, re_erange)) {
        return true;
    }

    // 4. Single-episode markers → definitively NOT a pack
    //    SxxEyy | NxNN | "E05"/"Ep 5" | anime "- 05" (max 3 digits so years
    //    like "- 2015" can't match)
    static const std::regex re_single(
        R"(\bS\d{1,2}\s*E\d{1,3}\b|\b\d{1,2}x\d{2,3}\b|\bEp?\.?\s?\d{1,3}\b|(?:^|\s)-\s\d{1,3}(?:v\d)?(?:\s|$))",
        std::regex::icase | std::regex::optimize);
    if (std::regex_search(n, re_single)) return false;

    // 5. Season mentioned with no episode marker: "Season 2", bare "S02"
    static const std::regex re_season_only(
        R"(\bseason\s*\d{1,2}\b|\bS\d{1,2}\b)",
        std::regex::icase | std::regex::optimize);
    return std::regex_search(n, re_season_only);
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

// ─────────────────────────────────────────────────────────────────────────────
// § 13  Structured name parsing — implementation (type declared in § 13a)
//
// Strategy: subtractive isolation with masking. Work on normalize(raw_name),
// and every time an entity is extracted, overwrite its span with SPACES rather
// than erasing it — indices stay stable, so `first_struct` (the smallest start
// index of any STRUCTURAL match) keeps meaning the same thing throughout, and
// later steps cannot re-match what an earlier step already claimed.
//
//   title = cleaned prefix [0, first_struct)
//
// "Structural" = a token that can only be metadata, so everything from it to
// the end of the name is metadata too. A leading "[Group]" and a title that
// happens to be a year are deliberately NOT structural.
//
// Extraction order is load-bearing:
//   1 leading [Group]  – gates the anime absolute-episode rule (step 5, last)
//   2 WxH resolution   – before year, or "1920x1080" donates the year 1920
//                        (and it is why "1x02" in step 5c is safe)
//   3 NNNNp resolution – value from parseResolution(), span masked here
//   4 daily date       – before year, which would otherwise eat its year part
//   5 season/episode   – first alternative that hits wins
//   6 year             – last match wins (scene rule: title precedes year)
//   7 technical tokens – codec/HDR/source/audio/language/edition vocabulary
//   8 trailing -GROUP  – matched on the UNMASKED name (see the comment there)
//   9 title assembly
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

// Overwrites [pos, pos+len) with spaces. Length-preserving on purpose: masking
// must not shift any index that first_struct or a later match may refer to.
inline void maskSpan(std::string& s, size_t pos, size_t len) {
    const size_t end = std::min(s.size(), pos + len);
    for (size_t i = pos; i < end; ++i) s[i] = ' ';
}

// Only these pixel heights are resolutions. Guards the WxH form against
// arbitrary "NNNxNNN" numbers that are not a video mode.
inline int heightToResolution(int h) {
    switch (h) {
        case 4320: case 2160: case 1080: case 720: case 576: case 480: return h;
        default: return 0;
    }
}

// Drops bracket characters left behind by masking, collapses whitespace and
// trims orphan separators. Internal hyphens survive — "9-1-1" is a real title.
inline std::string cleanTitle(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool prev_space = true;
    for (char c : s) {
        const bool sep = (c == '[' || c == ']' || c == '(' || c == ')' ||
                          c == '{' || c == '}' || c == ' ' || c == '\t');
        if (sep) {
            if (!prev_space) { out += ' '; prev_space = true; }
        } else {
            out += c;
            prev_space = false;
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '-' ||
                            out.back() == '_' || out.back() == ',' ||
                            out.back() == ':')) {
        out.pop_back();
    }
    size_t b = 0;
    while (b < out.size() && (out[b] == ' ' || out[b] == '-' || out[b] == '_')) ++b;
    return out.substr(b);
}

} // namespace detail

inline ParsedName parse(const std::string& raw_name) {
    ParsedName p;

    // `original` keeps the un-masked view; step 8 needs it.
    const std::string original = normalize(raw_name);
    std::string work = original;

    size_t first_struct = std::string::npos;
    const auto markStruct = [&first_struct](size_t pos) {
        if (pos < first_struct) first_struct = pos;
    };

    std::smatch m;

    // ── 1. Leading bracket group (fansub convention) ─────────────────────────
    // "[SubsPlease] Show - 05 ..." — NOT structural: the title comes AFTER it,
    // so cutting here would leave nothing. 2..32 chars rejects "[]" and the
    // long bracketed tag dumps that trail anime names.
    static const std::regex re_lead_group(
        R"(^\s*\[([^\]]{2,32})\])",
        std::regex::icase | std::regex::optimize);

    bool had_leading_group = false;
    if (std::regex_search(work, m, re_lead_group)) {
        p.release_group   = m[1].str();
        had_leading_group = true;
        detail::maskSpan(work, static_cast<size_t>(m.position(0)), m.str(0).size());
    }

    // ── 2. "WxH" resolution ──────────────────────────────────────────────────
    // Must run before the year step: "1920x1080" contains 1920, which the year
    // regex would happily take. Masking it is also the only reason the "1x02"
    // pattern (step 5c) cannot misfire on it.
    //   matches: "1920x1080"  "[1280 x 720]"      not: "x265" (no leading digits)
    static const std::regex re_wxh(
        R"((?:^|[\s\[\(])(\d{3,4})\s?[xX](\d{3,4})(?=[\s\]\)]|$))",
        std::regex::icase | std::regex::optimize);

    if (std::regex_search(work, m, re_wxh)) {
        p.resolution_p = detail::heightToResolution(std::stoi(m[2].str()));
        markStruct(static_cast<size_t>(m.position(0)));
        detail::maskSpan(work, static_cast<size_t>(m.position(0)), m.str(0).size());
    }

    // ── 3. "NNNNp" resolution tokens ─────────────────────────────────────────
    // Same token set and separator guards as parseResolution() (§ 2) — the
    // VALUE still comes from that one function; this copy exists to locate and
    // mask the span, which is what keeps "1080p" out of the title and feeds
    // first_struct. Every occurrence is masked ("[1080p][HEVC]" style names).
    static const std::regex re_res_p(
        R"((?:^|[\s\[\(\-\,])(?:4320|2160|1080|720|576|480)p(?=[\s\]\)\.\,_]|$))",
        std::regex::icase | std::regex::optimize);
    {
        std::vector<std::pair<size_t, size_t>> spans;
        for (auto it = std::sregex_iterator(work.begin(), work.end(), re_res_p),
                  end = std::sregex_iterator(); it != end; ++it) {
            spans.emplace_back(static_cast<size_t>(it->position(0)), it->str(0).size());
        }
        if (!spans.empty()) {
            const int res = parseResolution(work);
            if (res > 0) p.resolution_p = res;   // the "NNNNp" form outranks WxH
            markStruct(spans.front().first);
            for (const auto& [pos, len] : spans) detail::maskSpan(work, pos, len);
        }
    }

    // ── 4. Daily air date ────────────────────────────────────────────────────
    // Before the year step, which would otherwise consume the year part and
    // leave "03 15" behind. Post-normalize "2024.03.15" arrives as "2024 03 15".
    // Month/day are range-checked so "Blade Runner 2049 2017 2160p" (year then
    // a 2-digit-looking run) can never be mistaken for a date.
    static const std::regex re_daily(
        R"((?:^|[\s\[\(])((?:19|20)\d{2})[\s\.\-](\d{2})[\s\.\-](\d{2})(?=[\s\]\)]|$))",
        std::regex::icase | std::regex::optimize);

    if (std::regex_search(work, m, re_daily)) {
        const int mo = std::stoi(m[2].str());
        const int da = std::stoi(m[3].str());
        if (mo >= 1 && mo <= 12 && da >= 1 && da <= 31) {
            p.is_daily = true;
            p.air_date = m[1].str() + "-" + m[2].str() + "-" + m[3].str();
            markStruct(static_cast<size_t>(m.position(0)));
            detail::maskSpan(work, static_cast<size_t>(m.position(0)), m.str(0).size());
        }
    }

    // ── 5. Season / episode — first alternative that hits wins ───────────────
    // a. "S05E14", with an optional episode range "S01E01-E13"
    static const std::regex re_sxe(
        R"(\bS(\d{1,2})\s?E(\d{1,3})(?:\s?-\s?E?(\d{1,3}))?\b)",
        std::regex::icase | std::regex::optimize);
    // b. season range "S01-S05" / "S01-05" (reached only when (a) missed, so a
    //    pack name like "S01E01-E13" is already claimed by (a))
    static const std::regex re_srange(
        R"(\bS(\d{1,2})\s?-\s?S?(\d{1,2})\b)",
        std::regex::icase | std::regex::optimize);
    // c. "1x02" — safe ONLY because step 2 already masked any "1920x1080"
    static const std::regex re_nxnn(
        R"((?:^|[\s\[\(])(\d{1,2})x(\d{2,3})(?=[\s\]\)]|$))",
        std::regex::icase | std::regex::optimize);
    // d. spelled out: "Season 2", "Season 1 Episode 5"
    static const std::regex re_season_word(
        R"(\bSeasons?\s?(\d{1,2})(?:\s?Episode\s?(\d{1,3}))?\b)",
        std::regex::icase | std::regex::optimize);
    // e. episode range in brackets: "(01-24)", "[01~24]"  ⇒ pack
    static const std::regex re_ep_range(
        R"([\[\(]\s*0*(\d{1,3})\s?[-~]\s?0*(\d{1,3})\s*[\]\)])",
        std::regex::icase | std::regex::optimize);
    // f. "E455", "Ep 5", "Episode 12". "EXTENDED"/"EAC3" cannot match: the
    //    character after E must be a digit (or the literal "p"/"pisode").
    static const std::regex re_ep_word(
        R"(\bEp(?:isode)?\.?\s?0*(\d{1,3})\b|\bE0*(\d{1,3})\b)",
        std::regex::icase | std::regex::optimize);
    // g. anime absolute numbering " - 28", "- 05v2" — fansub-only, hence the
    //    had_leading_group gate: scene names never use it. § 11 guard set:
    //    the '-' must follow whitespace/start (kills "x265", "Dual-Audio",
    //    "Spider-Man") and the digits must be followed by a separator (kills
    //    "1080p" → 'p', "10bit" → 'b', and any longer number).
    static const std::regex re_abs_dash(
        R"((?:^|\s)-\s?0*(\d{1,4})(?:v\d+)?(?=[\s\[\(\)\]]|$))",
        std::regex::icase | std::regex::optimize);
    // h. bracketed absolute "[13]", "[13v2]"
    static const std::regex re_abs_bracket(
        R"(\[0*(\d{1,3})(?:v\d+)?\])",
        std::regex::icase | std::regex::optimize);

    bool se_found = false;
    if (std::regex_search(work, m, re_sxe)) {
        p.season  = std::stoi(m[1].str());
        p.episode = std::stoi(m[2].str());
        if (m[3].matched) {
            const int e2 = std::stoi(m[3].str());
            if (e2 > p.episode) p.episode_end = e2;
        }
        se_found = true;
    } else if (std::regex_search(work, m, re_srange)) {
        p.season = std::stoi(m[1].str());
        const int s2 = std::stoi(m[2].str());
        if (s2 > p.season) p.season_end = s2;
        se_found = true;
    } else if (std::regex_search(work, m, re_nxnn)) {
        p.season  = std::stoi(m[1].str());
        p.episode = std::stoi(m[2].str());
        se_found = true;
    } else if (std::regex_search(work, m, re_season_word)) {
        p.season = std::stoi(m[1].str());
        if (m[2].matched) p.episode = std::stoi(m[2].str());
        se_found = true;
    } else if (std::regex_search(work, m, re_ep_range)) {
        p.episode = std::stoi(m[1].str());
        const int e2 = std::stoi(m[2].str());
        if (e2 > p.episode) p.episode_end = e2;
        se_found = true;
    } else if (std::regex_search(work, m, re_ep_word)) {
        p.episode = std::stoi((m[1].matched ? m[1] : m[2]).str());
        se_found = true;
    } else if (had_leading_group && std::regex_search(work, m, re_abs_dash)) {
        p.episode = std::stoi(m[1].str());
        se_found = true;
    } else if (had_leading_group && std::regex_search(work, m, re_abs_bracket)) {
        const int v = std::stoi(m[1].str());
        // A bare bracketed resolution ("[720]", "[1080]") is not an episode.
        if (v != 480 && v != 576 && v != 720 && v != 1080) {
            p.episode = v;
            se_found  = true;
        }
    }
    if (se_found) {
        markStruct(static_cast<size_t>(m.position(0)));
        detail::maskSpan(work, static_cast<size_t>(m.position(0)), m.str(0).size());
    }

    // ── 6. Year ──────────────────────────────────────────────────────────────
    // Runs last of the numeric extractors, so WxH / dates / SxxEyy / NNNNp are
    // already masked. The LAST match wins — scene names put the title before
    // the year: "Blade Runner 2049 2017" → 2017, "2001 A Space Odyssey 1968"
    // → 1968.
    // Exception: a year in the FIRST token is the title ("2012 1080p BluRay").
    // It stays unmasked and non-structural so the title survives.
    static const std::regex re_year(
        R"((?:^|[\s\[\(])((?:19|20)\d{2})(?=[\s\]\)]|$))",
        std::regex::optimize);
    {
        size_t y_pos = 0, y_len = 0, y_tok = 0;
        int    y_val = 0;
        for (auto it = std::sregex_iterator(work.begin(), work.end(), re_year),
                  end = std::sregex_iterator(); it != end; ++it) {
            y_pos = static_cast<size_t>(it->position(0));
            y_len = it->str(0).size();
            y_tok = static_cast<size_t>(it->position(1));
            y_val = std::stoi(it->str(1));
        }
        // normalize() space-pads, so the first token starts at index 1.
        if (y_val > 0 && y_tok > 1) {
            p.year = y_val;
            markStruct(y_pos);
            detail::maskSpan(work, y_pos, y_len);
        }
    }

    // ── 7. Technical / noise vocabulary ──────────────────────────────────────
    // Values still come from § 3–§ 6; this pass only locates and masks the
    // tokens so they feed first_struct and never leak into the title.
    // Deliberately conservative: a false positive here TRUNCATES the title at
    // the token, so words that also occur in real titles are excluded —
    // no bare "WEB" ("Charlotte's Web"), "CAM" ("Cam", 2018), "MAX"
    // ("Mad Max"), "NF", "VF", "TS", "DV". Their compound forms (WEB-DL,
    // WEBRip, HDCAM, HDTS, Dolby Vision) carry the same signal without the
    // ambiguity, and isGarbage() (§ 6) still sees the bare forms for scoring.
    static const std::regex re_noise(
        R"(\b(?:)"
        R"(AV1|HEVC|x26[45]|H\s?26[45]|XviD|DivX|10bit|8bit|)"                  // codec
        R"(HDR10(?:\+|Plus)?|HDR|Dolby\s?Vision|)"                              // hdr
        R"(REMUX|BDRemux|BluRay|BLU\-?RAY|BDRip|BRRip|BDMux|HDRip|DVDRip|)"     // source
        R"(DVDScr|WEB\-?DL|WEBDL|WEBMux|WEBRip|HDTV|UHD|)"
        R"(CAMRip|HDCAM|HDTS|TeleSync|WORKPRINT|SCREENER|)"                     // garbage
        R"(AAC\d?|AC3|EAC3|DDP?\d?|DTS(?:\-?HD)?|TrueHD|Atmos|FLAC|OPUS|)"      // audio
        R"(MULTi|VOSTFR|TRUEFRENCH|FRENCH|SUBFRENCH|VFF|VFQ|VFHQ|SUBBED|)"      // language
        R"(ENGSUB|Dual\s?Audio|Multi\s?Audio|Multi\s?Sub|Subtitle|)"
        R"(PROPER|REPACK|EXTENDED|REMASTERED|Batch|Complete|Collection|)"       // edition
        R"(Integrale?|)"
        R"(AMZN|DSNP|HMAX|ATVP|PCOK)"                                           // services
        R"()\b)",
        std::regex::icase | std::regex::optimize);
    {
        std::vector<std::pair<size_t, size_t>> spans;
        for (auto it = std::sregex_iterator(work.begin(), work.end(), re_noise),
                  end = std::sregex_iterator(); it != end; ++it) {
            spans.emplace_back(static_cast<size_t>(it->position(0)), it->str(0).size());
        }
        if (!spans.empty()) {
            markStruct(spans.front().first);
            for (const auto& [pos, len] : spans) detail::maskSpan(work, pos, len);
        }
    }

    // ── 8. Trailing scene group ──────────────────────────────────────────────
    // std::regex has no lookbehind, so the preceding character is captured and
    // required to be non-space and non-dash: that kills anime " - 13" and
    // "Title - Subtitle" while keeping "x264-SPARKS".
    //
    // Matched against `original`, NOT the masked string — two reasons:
    //   • step 7 masks "x264", which would leave "-SPARKS" with no prefix char;
    //   • masking a trailing "2021 1080p" promotes an earlier hyphen to the end
    //     of the string, so "Spider-Man No Way Home 2021 1080p" would hand back
    //     the group "Man" and amputate the title.
    static const std::regex re_tail_group(
        R"(([^\s\-])-([A-Za-z][A-Za-z0-9]{1,19})\s*$)",
        std::regex::optimize);
    // Technical tails are not group names: "...-x265", "WEB-DL", "Dual-Audio".
    static const std::regex re_not_a_group(
        R"(^(?:x26[45]|h26[45]|HEVC|AVC|AV1|DL|Rip|WEB|WEBRip|BluRay|REMUX|)"
        R"(HDR10?|DV|FLAC|AAC|Audio|Sub(?:s|title)?|\d{3,4}p)$)",
        std::regex::icase | std::regex::optimize);

    if (p.release_group.empty() && std::regex_search(original, m, re_tail_group)) {
        const std::string cand = m[2].str();
        if (!std::regex_match(cand, re_not_a_group)) {
            p.release_group = cand;
            const size_t dash = static_cast<size_t>(m.position(0)) + m.str(1).size();
            markStruct(dash);
            detail::maskSpan(work, dash, m.str(0).size() - m.str(1).size());
        }
    }

    // ── 9. Title assembly ────────────────────────────────────────────────────
    const size_t cut = (first_struct == std::string::npos) ? work.size() : first_struct;
    p.title = detail::cleanTitle(work.substr(0, cut));
    // Nothing before the first structural token (e.g. "S01E01 1080p Show"):
    // fall back to whatever survived masking anywhere in the name.
    if (p.title.empty()) p.title = detail::cleanTitle(work);
    return p;
}

} // namespace media::services::TorrentScorer
