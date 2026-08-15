// parser_tests.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Fixture suite for TorrentScorer::parse() (§ 13) and its wiring into enrich().
//
// TorrentScorer.hpp is header-only and std-only, so this binary needs no Conan
// dependencies and builds anywhere:
//
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
//       -Iserver/include server/tests/parser_tests.cpp -o parser_tests
//   ./parser_tests
//
// or through CMake:
//
//   cmake -B build -DBUILD_PARSER_TESTS=ON && cmake --build build -j
//   ./build/parser_tests
//
// Exit code = number of failed assertions.
// ─────────────────────────────────────────────────────────────────────────────

#include "mediastream/services/TorrentScorer.hpp"

#include <iostream>
#include <string>

namespace {

using media::services::TorrentQuality;
using media::services::TorrentScorer::ParsedName;

int g_failures = 0;
int g_checks   = 0;
const char* g_case = "";

void fail(const std::string& what, const std::string& got, const std::string& want,
          int line) {
    ++g_failures;
    std::cout << "FAIL  parser_tests.cpp:" << line << "  [" << g_case << "] " << what
              << "\n        got: " << got
              << "\n       want: " << want << '\n';
}

void checkStr(const std::string& got, const std::string& want,
              const char* what, int line) {
    ++g_checks;
    if (got != want) fail(what, "\"" + got + "\"", "\"" + want + "\"", line);
}

void checkInt(int got, int want, const char* what, int line) {
    ++g_checks;
    if (got != want) fail(what, std::to_string(got), std::to_string(want), line);
}

void checkBool(bool got, bool want, const char* what, int line) {
    ++g_checks;
    if (got != want) fail(what, got ? "true" : "false", want ? "true" : "false", line);
}

#define CASE(name)      g_case = (name)
#define EQ_S(a, b)      checkStr((a), (b), #a, __LINE__)
#define EQ_I(a, b)      checkInt((a), (b), #a, __LINE__)
#define EQ_B(a, b)      checkBool((a), (b), #a, __LINE__)

ParsedName P(const char* name) {
    CASE(name);
    return media::services::TorrentScorer::parse(name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene movies
// ─────────────────────────────────────────────────────────────────────────────
void testSceneMovies() {
    {
        auto p = P("The.Matrix.1999.1080p.BluRay.x264-SPARKS");
        EQ_S(p.title, "The Matrix");
        EQ_I(p.year, 1999);
        EQ_S(p.release_group, "SPARKS");
        EQ_I(p.resolution_p, 1080);
        EQ_I(p.season, 0);
        EQ_I(p.episode, 0);
    }
    {
        // Two year-shaped tokens: the LAST one is the year, "2049" is title.
        auto p = P("Blade.Runner.2049.2017.2160p.WEB-DL.HDR10");
        EQ_S(p.title, "Blade Runner 2049");
        EQ_I(p.year, 2017);
        EQ_I(p.resolution_p, 2160);
    }
    {
        auto p = P("2001.A.Space.Odyssey.1968.REMUX");
        EQ_S(p.title, "2001 A Space Odyssey");
        EQ_I(p.year, 1968);
    }
    {
        auto p = P("1917.2019.720p.WEBRip");
        EQ_S(p.title, "1917");
        EQ_I(p.year, 2019);
        EQ_I(p.resolution_p, 720);
    }
    {
        // First-token rule: a leading year IS the title, so year stays 0.
        auto p = P("2012.1080p.BluRay");
        EQ_S(p.title, "2012");
        EQ_I(p.year, 0);
        EQ_I(p.resolution_p, 1080);
    }
    {
        // WxH must be consumed before the year step, or 1920 becomes the year.
        auto p = P("Movie.Name.1920x1080.WEBRip");
        EQ_S(p.title, "Movie Name");
        EQ_I(p.year, 0);
        EQ_I(p.resolution_p, 1080);
    }
    {
        // Hyphenated title survives; the trailing-group rule must not claim
        // "-Man" once the tail tokens are masked.
        auto p = P("Spider-Man.No.Way.Home.2021.1080p");
        EQ_S(p.title, "Spider-Man No Way Home");
        EQ_I(p.year, 2021);
        EQ_S(p.release_group, "");
    }
    {
        auto p = P("Some.Film.2020.MULTi.VFF.1080p-GRP");
        EQ_S(p.title, "Some Film");
        EQ_I(p.year, 2020);
        EQ_S(p.release_group, "GRP");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Series
// ─────────────────────────────────────────────────────────────────────────────
void testSeries() {
    {
        auto p = P("Breaking.Bad.S05E14.1080p.WEB-DL");
        EQ_S(p.title, "Breaking Bad");
        EQ_I(p.season, 5);
        EQ_I(p.episode, 14);
        EQ_I(p.episode_end, 0);
        // "WEB-DL" is a source tag, not a release group.
        EQ_S(p.release_group, "");
    }
    {
        // Hyphens inside the title must survive the cleanup pass.
        auto p = P("9-1-1.S05E03.720p.HDTV");
        EQ_S(p.title, "9-1-1");
        EQ_I(p.season, 5);
        EQ_I(p.episode, 3);
    }
    {
        auto p = P("Show.Name.1x02.HDTV.x264");
        EQ_S(p.title, "Show Name");
        EQ_I(p.season, 1);
        EQ_I(p.episode, 2);
    }
    {
        // Pinned observed behavior: the year is extracted separately, so the
        // title comes back WITHOUT it. Consumers that want the disambiguated
        // name join `title` + `year` themselves.
        auto p = P("Doctor.Who.2005.S04E12.720p");
        EQ_S(p.title, "Doctor Who");
        EQ_I(p.year, 2005);
        EQ_I(p.season, 4);
        EQ_I(p.episode, 12);
    }
    {
        auto p = P("The.Daily.Show.2024.03.15.1080p");
        EQ_S(p.title, "The Daily Show");
        EQ_B(p.is_daily, true);
        EQ_S(p.air_date, "2024-03-15");
        EQ_I(p.year, 0);      // consumed by the date, never double-counted
        EQ_I(p.episode, 0);
    }
    {
        auto p = P("Show.S01E01-E13.Complete.1080p");
        EQ_S(p.title, "Show");
        EQ_I(p.season, 1);
        EQ_I(p.episode, 1);
        EQ_I(p.episode_end, 13);
        EQ_B(media::services::TorrentScorer::looksLikeSeasonPack(
                 "Show.S01E01-E13.Complete.1080p"), true);
    }
    {
        auto p = P("Series.S01-S05.COMPLETE.BluRay");
        EQ_S(p.title, "Series");
        EQ_I(p.season, 1);
        EQ_I(p.season_end, 5);
        EQ_I(p.episode, 0);
        EQ_B(media::services::TorrentScorer::looksLikeSeasonPack(
                 "Series.S01-S05.COMPLETE.BluRay"), true);
    }
    {
        auto p = P("Mr.Robot.Season.2.Complete");
        EQ_S(p.title, "Mr Robot");
        EQ_I(p.season, 2);
        EQ_I(p.episode, 0);
        EQ_B(media::services::TorrentScorer::looksLikeSeasonPack(
                 "Mr.Robot.Season.2.Complete"), true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Anime (Nyaa / fansub naming)
// ─────────────────────────────────────────────────────────────────────────────
void testAnime() {
    {
        auto p = P("[SubsPlease] Sousou no Frieren - 28 (1080p) [ABC123].mkv");
        EQ_S(p.title, "Sousou no Frieren");
        EQ_I(p.episode, 28);
        EQ_I(p.season, 0);
        EQ_S(p.release_group, "SubsPlease");
        EQ_I(p.resolution_p, 1080);
    }
    {
        // "05v2" — the version suffix is tolerated, the episode is still 5.
        auto p = P("[Erai-raws] Frieren - 05v2 [720p][Multiple Subtitle]");
        EQ_S(p.title, "Frieren");
        EQ_I(p.episode, 5);
        EQ_S(p.release_group, "Erai-raws");
        EQ_I(p.resolution_p, 720);
    }
    {
        // Season pack: "x265" / "10bit" must not leak in as an episode number.
        auto p = P("[Judas] Frieren (Season 1) [1080p][HEVC x265 10bit][Dual-Audio][Batch]");
        EQ_S(p.title, "Frieren");
        EQ_I(p.season, 1);
        EQ_I(p.episode, 0);
        EQ_I(p.resolution_p, 1080);
        EQ_B(media::services::TorrentScorer::looksLikeSeasonPack(
                 "[Judas] Frieren (Season 1) [1080p][HEVC x265 10bit][Dual-Audio][Batch]"),
             true);
    }
    {
        // Long-running shows really do reach episode 1080, so a bare " - 1080 "
        // IS the episode. The resolution comes from the "[1080p]" token, which
        // step 3 masks before the absolute-episode rule ever runs.
        auto p = P("[Group] Show - 1080 [1080p]");
        EQ_I(p.episode, 1080);
        EQ_I(p.resolution_p, 1080);
        EQ_S(p.title, "Show");
    }
    {
        auto p = P("[SubsPlease] Show (01-24) [Batch]");
        EQ_S(p.title, "Show");
        EQ_I(p.episode, 1);
        EQ_I(p.episode_end, 24);
        EQ_B(media::services::TorrentScorer::looksLikeSeasonPack(
                 "[SubsPlease] Show (01-24) [Batch]"), true);
    }
    {
        auto p = P("Naruto.Shippuden.E455.720p");
        EQ_S(p.title, "Naruto Shippuden");
        EQ_I(p.episode, 455);
        EQ_I(p.season, 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Negative guards — technical tokens must never become season/episode/year
// ─────────────────────────────────────────────────────────────────────────────
void testNegativeGuards() {
    const char* const kGarbage[] = {
        "x265", "x264", "1080p", "10bit", "AAC2.0", "DDP5.1", "HDR10", "H.265",
        "H.264", "DTS-HD.MA.5.1", "8bit", "2160p",
    };
    for (const char* g : kGarbage) {
        auto p = P(g);
        EQ_I(p.season, 0);
        EQ_I(p.episode, 0);
        EQ_I(p.episode_end, 0);
        EQ_I(p.year, 0);
        EQ_B(p.is_daily, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Real Torrentio / EZTV / Nyaa names.
//
// These exist to pin the title-truncation traps: the § 13 step-7 noise
// vocabulary cuts the title at its FIRST match, so any release word that is
// also an ordinary title word would amputate the title. "Charlottes Web",
// "Mad Max", "Cam" and "Star Wars Episode IV" are the canonical victims —
// if someone adds bare WEB / MAX / CAM / Episode to re_noise, these fail.
// ─────────────────────────────────────────────────────────────────────────────
void testRealWorldNames() {
    {
        auto p = P("The.Last.of.Us.S01E05.2160p.HMAX.WEB-DL.DDP5.1.Atmos.DV.HDR.H.265-SMURF");
        EQ_S(p.title, "The Last of Us");
        EQ_I(p.season, 1);
        EQ_I(p.episode, 5);
        EQ_I(p.resolution_p, 2160);
        EQ_S(p.release_group, "SMURF");
    }
    {
        auto p = P("Charlottes.Web.2006.1080p.WEBRip.x265-RARBG");
        EQ_S(p.title, "Charlottes Web");   // bare "WEB" must not be a noise token
        EQ_I(p.year, 2006);
        EQ_S(p.release_group, "RARBG");
    }
    {
        auto p = P("Mad.Max.Fury.Road.2015.1080p.BluRay.x264-SPARKS");
        EQ_S(p.title, "Mad Max Fury Road"); // bare "MAX" must not be a noise token
        EQ_I(p.year, 2015);
    }
    {
        auto p = P("Cam.2018.1080p.NF.WEB-DL.DDP5.1.x264-NTG");
        EQ_S(p.title, "Cam");               // bare "CAM" must not be a noise token
        EQ_I(p.year, 2018);
        EQ_S(p.release_group, "NTG");
    }
    {
        // "Episode IV" is a title word, not an episode marker: the E-rules all
        // require a digit right after the E / "Ep".
        auto p = P("Star.Wars.Episode.IV.A.New.Hope.1977.1080p.BluRay");
        EQ_S(p.title, "Star Wars Episode IV A New Hope");
        EQ_I(p.year, 1977);
        EQ_I(p.episode, 0);
    }
    {
        auto p = P("Oppenheimer.2023.2160p.UHD.BluRay.REMUX.DV.HDR.HEVC.TrueHD.7.1.Atmos-FraMeSToR");
        EQ_S(p.title, "Oppenheimer");
        EQ_I(p.year, 2023);
        EQ_S(p.release_group, "FraMeSToR");
    }
    {
        auto p = P("Game.of.Thrones.S01-S08.COMPLETE.1080p.BluRay.x265");
        EQ_S(p.title, "Game of Thrones");
        EQ_I(p.season, 1);
        EQ_I(p.season_end, 8);
        EQ_I(p.episode, 0);
    }
    {
        // "8-gou": the hyphen is followed by a letter, so the anime absolute
        // rule cannot claim it as an episode.
        auto p = P("[SubsPlease] Kaijuu 8-gou - 09 (1080p) [C7D0E6F5].mkv");
        EQ_S(p.title, "Kaijuu 8-gou");
        EQ_I(p.episode, 9);
        EQ_S(p.release_group, "SubsPlease");
    }
    {
        auto p = P("[Anime Time] One Piece - 1085 [1080p][HEVC 10bit x265][AAC][Multi Sub]");
        EQ_S(p.title, "One Piece");
        EQ_I(p.episode, 1085);
        EQ_I(p.resolution_p, 1080);
    }
    {
        auto p = P("9-1-1.Lone.Star.S04E10.1080p.WEB.h264-CAKES");
        EQ_S(p.title, "9-1-1 Lone Star");
        EQ_I(p.season, 4);
        EQ_I(p.episode, 10);
    }
    {
        auto p = P("La.Casa.de.Papel.S05E01.MULTi.1080p.NF.WEB-DL.DDP5.1.x264-TOPKEK");
        EQ_S(p.title, "La Casa de Papel");
        EQ_I(p.season, 5);
        EQ_I(p.episode, 1);
        EQ_S(p.release_group, "TOPKEK");
    }
    {
        auto p = P("Frieren.Beyond.Journeys.End.S01E28.1080p.CR.WEB-DL.AAC2.0.H.264-VARYG");
        EQ_S(p.title, "Frieren Beyond Journeys End");
        EQ_I(p.season, 1);
        EQ_I(p.episode, 28);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// enrich() wiring — the parse results reach TorrentQuality, and a range
// promotes is_pack without clobbering what the client already decided.
// ─────────────────────────────────────────────────────────────────────────────
void testEnrichWiring() {
    {
        CASE("enrich: scene movie");
        TorrentQuality tq;
        media::services::TorrentScorer::enrich(tq, "The.Matrix.1999.1080p.BluRay.x264-SPARKS");
        EQ_S(tq.parsed_title, "The Matrix");
        EQ_I(tq.year, 1999);
        EQ_S(tq.release_group, "SPARKS");
        EQ_I(tq.resolution_p, 1080);
        EQ_S(tq.codec, "x264");
        EQ_B(tq.is_bluray, true);
        EQ_S(tq.quality, "1080p");
        EQ_B(tq.is_pack, false);
    }
    {
        CASE("enrich: WxH resolution fallback");
        TorrentQuality tq;
        media::services::TorrentScorer::enrich(tq, "Movie.Name.1920x1080.WEBRip");
        EQ_I(tq.resolution_p, 1080);
        EQ_S(tq.quality, "1080p");
    }
    {
        CASE("enrich: episode range promotes is_pack");
        TorrentQuality tq;
        media::services::TorrentScorer::enrich(tq, "Show.S01E01-E13.1080p");
        EQ_I(tq.season, 1);
        EQ_I(tq.episode, 1);
        EQ_I(tq.episode_end, 13);
        EQ_B(tq.is_pack, true);
    }
    {
        CASE("enrich: season range promotes is_pack");
        TorrentQuality tq;
        media::services::TorrentScorer::enrich(tq, "Series.S01-S05.BluRay");
        EQ_I(tq.season, 1);
        EQ_B(tq.is_pack, true);
    }
    {
        CASE("enrich: single episode never clears a client's is_pack");
        TorrentQuality tq;
        tq.is_pack = true;  // set by looksLikeSeasonPack on the TORRENT-level name
        media::services::TorrentScorer::enrich(tq, "Show.S01E02.1080p.WEB-DL");
        EQ_B(tq.is_pack, true);
        EQ_I(tq.episode, 2);
        EQ_I(tq.episode_end, 0);
    }
    {
        CASE("enrich: unparseable name degrades quietly");
        TorrentQuality tq;
        media::services::TorrentScorer::enrich(tq, "");
        EQ_S(tq.parsed_title, "");
        EQ_I(tq.year, 0);
        EQ_S(tq.quality, "unknown");
    }
}

} // namespace

int main() {
    testSceneMovies();
    testSeries();
    testAnime();
    testNegativeGuards();
    testRealWorldNames();
    testEnrichWiring();

    std::cout << (g_failures == 0 ? "PASS" : "FAILED") << "  "
              << (g_checks - g_failures) << "/" << g_checks << " assertions\n";
    return g_failures;
}
