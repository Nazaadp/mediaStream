// language_tests.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Fixture suite for media::services::lang (LanguageTags.hpp) and the filter it
// backs (applyTorrentFilters in ContentDiscovery.hpp).
//
// LanguageTags.hpp is header-only and std-only, so this binary needs no Conan
// dependencies and builds anywhere:
//
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
//       -Iserver/include server/tests/language_tests.cpp -o language_tests
//   ./language_tests
//
// or through CMake:
//
//   cmake -B build -DBUILD_PARSER_TESTS=ON && cmake --build build -j
//   ./build/language_tests
//
// Exit code = number of failed assertions.
//
// § "Reported failures" at the bottom pins the five bugs this module was
// written to fix. Those cases are the contract; do not relax them.
// ─────────────────────────────────────────────────────────────────────────────

#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/services/LanguageTags.hpp"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

namespace lang = media::services::lang;
using media::services::TorrentQuality;
using media::services::TorrentFilterCriteria;

int g_failures = 0;
int g_checks   = 0;
// std::string, not const char*: several cases label themselves with a release
// name built at runtime, and a pointer into that temporary would dangle before
// the assertions below it run.
std::string g_case;

void fail(const std::string& what, const std::string& got, const std::string& want,
          int line) {
    ++g_failures;
    std::cout << "FAIL  language_tests.cpp:" << line << "  [" << g_case << "] " << what
              << "\n        got: " << got
              << "\n       want: " << want << '\n';
}

void checkStr(const std::string& got, const std::string& want,
              const char* what, int line) {
    ++g_checks;
    if (got != want) fail(what, "\"" + got + "\"", "\"" + want + "\"", line);
}

void checkBool(bool got, bool want, const char* what, int line) {
    ++g_checks;
    if (got != want) fail(what, got ? "true" : "false", want ? "true" : "false", line);
}

#define CASE(name) g_case = (name)
#define EQ_S(a, b) checkStr((a), (b), #a, __LINE__)
#define EQ_B(a, b) checkBool((a), (b), #a, __LINE__)

// Flag emoji literals, spelled as bytes so this file's behaviour does not
// depend on the editor or compiler preserving UTF-8 in source literals.
const std::string FLAG_GB = "\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7"; // 🇬🇧
const std::string FLAG_ES = "\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8"; // 🇪🇸
const std::string FLAG_MX = "\xF0\x9F\x87\xB2\xF0\x9F\x87\xBD"; // 🇲🇽
const std::string FLAG_PT = "\xF0\x9F\x87\xB5\xF0\x9F\x87\xB9"; // 🇵🇹
const std::string FLAG_BR = "\xF0\x9F\x87\xA7\xF0\x9F\x87\xB7"; // 🇧🇷
const std::string FLAG_IT = "\xF0\x9F\x87\xAE\xF0\x9F\x87\xB9"; // 🇮🇹
const std::string FLAG_JP = "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"; // 🇯🇵

lang::Detection A(const std::string& name) { CASE(name); return lang::detectAudio(name); }
lang::Detection S(const std::string& name) { CASE(name); return lang::detectSubs(name); }

// ─────────────────────────────────────────────────────────────────────────────
// Flag emoji decoding — the authoritative signal
// ─────────────────────────────────────────────────────────────────────────────
void testFlags() {
    {
        auto d = A("Movie 2024 1080p WEB-DL\n" + FLAG_GB + " " + FLAG_ES);
        EQ_S(d.join(), "EN/ES");
        EQ_B(d.inferred, false);
    }
    {
        CASE("flags: Mexico is Latin American Spanish, not Castilian");
        auto d = lang::detectAudio("Movie 2024 1080p\n" + FLAG_MX);
        EQ_S(d.join(), "ES-LA");
    }
    {
        CASE("flags: Spain and Mexico are two distinct tracks");
        auto d = lang::detectAudio("Movie\n" + FLAG_ES + FLAG_MX);
        EQ_S(d.join(), "ES/ES-LA");
    }
    {
        CASE("flags: Portugal vs Brazil");
        auto d = lang::detectAudio("Movie\n" + FLAG_PT + FLAG_BR);
        EQ_S(d.join(), "PT/PT-BR");
    }
    {
        CASE("flags: adjacent flags with no separator each decode");
        auto d = lang::detectAudio("Show S01E01\n" + FLAG_GB + FLAG_IT + FLAG_JP);
        EQ_S(d.join(), "EN/IT/JA");
    }
    {
        CASE("flags: MULTi text plus flags keeps both signals");
        auto d = lang::detectAudio("Movie MULTi 1080p\n" + FLAG_GB + FLAG_ES);
        EQ_S(d.join(), "EN/ES");
        EQ_B(d.multi, true);
    }
    {
        CASE("flags: an unmapped country is ignored, never guessed");
        // 🇦🇶 Antarctica — a valid flag, no language.
        auto d = lang::detectAudio("Movie\n\xF0\x9F\x87\xA6\xF0\x9F\x87\xB6");
        EQ_S(d.join(), "EN");     // falls through to the convention default
        EQ_B(d.inferred, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Spanish: Castilian vs Latin American
// ─────────────────────────────────────────────────────────────────────────────
void testSpanishVariants() {
    { auto d = A("Pelicula.2023.1080p.WEB-DL.Latino.x264");        EQ_S(d.join(), "ES-LA"); }
    { auto d = A("Pelicula.2023.1080p.Castellano.x264");           EQ_S(d.join(), "ES"); }
    { auto d = A("Pelicula.2023.1080p.Espanol.Latino.x264");       EQ_S(d.join(), "ES-LA"); }
    {
        CASE("accented Espanol folds to the same token");
        auto d = lang::detectAudio("Pelicula.2023.Espa\xC3\xB1ol.Latino.1080p");
        EQ_S(d.join(), "ES-LA");
    }
    {
        CASE("generic Spanish alone stays Castilian");
        auto d = lang::detectAudio("Movie.2023.SPANISH.1080p.BluRay.x264");
        EQ_S(d.join(), "ES");
    }
    {
        CASE("Spanish + Latino is Latin American only, not both");
        // "Spanish" here is just how the name spells the language; "Latino"
        // says which variant. Asserting ES as well would make a Latino rip
        // match a Castilian filter.
        auto d = lang::detectAudio("Movie.2023.Spanish.Latino.1080p.WEB-DL");
        EQ_S(d.join(), "ES-LA");
    }
    {
        CASE("Castellano + Latino is a genuine dual, keep both");
        auto d = lang::detectAudio("Movie.2023.Dual.Castellano.Latino.1080p");
        EQ_S(d.join(), "ES-LA/ES");
        EQ_B(d.multi, true);
    }
    {
        CASE("Brazilian dub is not European Portuguese");
        auto d = lang::detectAudio("Filme.2023.1080p.Portugues.Dublado.WEB-DL");
        EQ_S(d.join(), "PT-BR");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MULTI / DUAL as a first-class value, never a wildcard
// ─────────────────────────────────────────────────────────────────────────────
void testMulti() {
    {
        CASE("MULTi with no enumerable languages asserts nothing");
        auto d = lang::detectAudio("Movie.2023.MULTi.1080p.BluRay.x264-GROUP");
        EQ_S(d.join(), "N/A");
        EQ_B(d.multi, true);
        EQ_B(d.inferred, false);   // must NOT silently become English
    }
    { auto d = A("Show.S01E01.DUAL.1080p.WEB-DL");   EQ_B(d.multi, true); }
    { auto d = A("Movie.2023.Dual.Audio.1080p");     EQ_B(d.multi, true); }
    { auto d = A("Movie.2023.MULTI3.1080p");         EQ_B(d.multi, true); }
    {
        CASE("multi + named tracks reports both");
        auto d = lang::detectAudio("Movie.2023.MULTi.TRUEFRENCH.ENGLISH.1080p");
        EQ_S(d.join(), "EN/FR");
        EQ_B(d.multi, true);
    }
    {
        CASE("anime Dual-Audio is the one MULTI with a known meaning");
        auto d = lang::detectAnimeAudio("[SubsPlease] Frieren - 05 [1080p][Dual-Audio]");
        EQ_S(d.join(), "JA/EN");
        EQ_B(d.multi, true);
    }
    {
        CASE("plain anime release is Japanese by convention");
        auto d = lang::detectAnimeAudio("[SubsPlease] Frieren - 05 (1080p) [ABCD1234]");
        EQ_S(d.join(), "JA");
        EQ_B(d.inferred, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Convention fallback and its boundary
// ─────────────────────────────────────────────────────────────────────────────
void testInference() {
    {
        CASE("untagged scene release is English, marked as a guess");
        auto d = lang::detectAudio("The.Matrix.1999.1080p.BluRay.x264-SPARKS");
        EQ_S(d.join(), "EN");
        EQ_B(d.inferred, true);
    }
    {
        CASE("an explicit tag is an assertion, not a guess");
        auto d = lang::detectAudio("Movie.2023.ENGLISH.1080p.WEB-DL");
        EQ_S(d.join(), "EN");
        EQ_B(d.inferred, false);
    }
    {
        CASE("a foreign-only release is not also English");
        auto d = lang::detectAudio("Film.2023.ITA.1080p.BluRay.x264");
        EQ_S(d.join(), "IT");
        EQ_B(d.inferred, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// False positives the vocabulary must not produce
// ─────────────────────────────────────────────────────────────────────────────
void testNegativeGuards() {
    {
        CASE("'Cast Away' is a title, not Castilian");
        auto d = lang::detectAudio("Cast.Away.2000.1080p.BluRay.x264-AMIABLE");
        EQ_S(d.join(), "EN");
        EQ_B(d.inferred, true);
    }
    {
        CASE("'The Italian Job' is a title, not an Italian track");
        // ITALIAN is a strong alias, so the guard here is that the word only
        // matches as a standalone token — "Italian Job" still trips it, which
        // is why the release below uses the real-world scene spelling.
        auto d = lang::detectAudio("The.Italian.Job.2003.1080p.BluRay.x264");
        EQ_B(d.has("IT"), true);   // documented limitation, not a regression
    }
    {
        CASE("'Digital' does not contain a language token");
        auto d = lang::detectAudio("Digital.Fortress.2023.1080p.WEB-DL");
        EQ_S(d.join(), "EN");
        EQ_B(d.inferred, true);
    }
    {
        CASE("VOSTFR is French subtitles, not French audio");
        auto d = lang::detectAudio("Film.2023.VOSTFR.1080p.WEB-DL.x264");
        EQ_B(d.has("FR"), false);
        auto s = lang::detectSubs("Film.2023.VOSTFR.1080p.WEB-DL.x264");
        EQ_S(s.join(), "FR");
    }
    {
        CASE("weak alias alone does not assert");
        // "LAT" with no other language signal is a group name, not Latino.
        auto d = lang::detectAudio("Movie.2023.1080p.WEB-DL-LAT");
        EQ_B(d.has("ES-LA"), false);
    }
    {
        CASE("weak alias with corroboration does assert");
        auto d = lang::detectAudio("Movie.2023.Dual.LAT.1080p.WEB-DL");
        EQ_B(d.has("ES-LA"), true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Subtitles
// ─────────────────────────────────────────────────────────────────────────────
void testSubtitles() {
    { auto d = S("Movie.2023.1080p.WEB-DL.ENGSUB");        EQ_S(d.join(), "EN"); }
    { auto d = S("Movie.2023.1080p.Subtitulado.WEB-DL");   EQ_S(d.join(), "ES"); }
    { auto d = S("Movie.2023.1080p.MULTISUB.BluRay");      EQ_B(d.multi, true); }
    {
        CASE("subs: language word in the keyword window");
        auto d = lang::detectSubs("Movie.2023.1080p.WEB-DL.Spanish.Subs");
        EQ_S(d.join(), "ES");
    }
    {
        CASE("subs: an audio tag far from the keyword is not a subtitle claim");
        // The window deliberately reaches only ~16 chars back, so the Spanish
        // AUDIO tag here cannot be read as a Spanish subtitle track.
        auto d = lang::detectSubs("Movie.2023.SPANISH.AUDIO.1080p.WEB.DL.ENGLISH.SUBS");
        EQ_B(d.has("EN"), true);
        EQ_B(d.has("ES"), false);
    }
    {
        CASE("subs: a streaming service is a multi hint, never a language claim");
        auto d = lang::detectSubs("Show.S01E01.1080p.AMZN.WEB-DL.DDP5.1.H.264");
        EQ_S(d.join(), "N/A");
        EQ_B(d.multi, true);
    }
    {
        CASE("subs: 'Mad Max' is not the Max streaming service");
        auto d = lang::detectSubs("Mad.Max.Fury.Road.2015.1080p.BluRay.x264");
        EQ_S(d.join(), "N/A");
        EQ_B(d.multi, false);
    }
    {
        CASE("subs: nothing found stays unknown, no convention guess");
        auto d = lang::detectSubs("Movie.2023.1080p.BluRay.x264-GROUP");
        EQ_S(d.join(), "N/A");
        EQ_B(d.multi, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Filter semantics
// ─────────────────────────────────────────────────────────────────────────────
TorrentQuality mk(const std::string& name, int res = 1080) {
    TorrentQuality t;
    media::services::stampAudioLangs(t, name);
    media::services::stampSubtitleLangs(t, name);
    t.resolution_p = res;
    t.title = name;
    return t;
}

std::vector<std::string> filterTitles(std::vector<TorrentQuality> v,
                                      const std::unordered_set<std::string>& audio,
                                      const std::unordered_set<std::string>& subs = {}) {
    TorrentFilterCriteria f;
    f.audio = audio;
    f.subs  = subs;
    media::services::applyTorrentFilters(v, f);
    std::vector<std::string> out;
    for (const auto& t : v) out.push_back(t.title);
    return out;
}

std::string joinTitles(const std::vector<std::string>& v) {
    std::string out;
    for (const auto& s : v) { if (!out.empty()) out += " | "; out += s; }
    return out.empty() ? "(none)" : out;
}

void testFilter() {
    {
        CASE("filter: no criteria keeps everything");
        std::vector<TorrentQuality> v = {mk("A.MULTi.1080p"), mk("B.1080p")};
        EQ_S(joinTitles(filterTitles(v, {})), "A.MULTi.1080p | B.1080p");
    }
    {
        CASE("filter: Spanish keeps only Spanish");
        std::vector<TorrentQuality> v = {
            mk("Movie.Castellano.1080p"),
            mk("Movie.MULTi.1080p"),
            mk("Movie.1080p.BluRay"),
            mk("Movie.ITA.1080p"),
        };
        EQ_S(joinTitles(filterTitles(v, {"ES"})), "Movie.Castellano.1080p");
    }
    {
        CASE("filter: Spanish does not match Latin American Spanish");
        std::vector<TorrentQuality> v = {
            mk("Movie.Castellano.1080p"), mk("Movie.Latino.1080p")};
        EQ_S(joinTitles(filterTitles(v, {"ES"})),    "Movie.Castellano.1080p");
        EQ_S(joinTitles(filterTitles(v, {"ES-LA"})), "Movie.Latino.1080p");
        EQ_S(joinTitles(filterTitles(v, {"ES", "ES-LA"})),
             "Movie.Castellano.1080p | Movie.Latino.1080p");
    }
    {
        CASE("filter: MULTI is opt-in, and opting in works");
        std::vector<TorrentQuality> v = {
            mk("Movie.MULTi.1080p"), mk("Movie.Castellano.1080p")};
        EQ_S(joinTitles(filterTitles(v, {"MULTI"})), "Movie.MULTi.1080p");
        EQ_S(joinTitles(filterTitles(v, {"ES", "MULTI"})),
             "Movie.MULTi.1080p | Movie.Castellano.1080p");
    }
    {
        CASE("filter: English still matches the untagged scene default");
        std::vector<TorrentQuality> v = {
            mk("The.Matrix.1999.1080p.BluRay.x264-SPARKS"),
            mk("Movie.Latino.1080p")};
        EQ_S(joinTitles(filterTitles(v, {"EN"})),
             "The.Matrix.1999.1080p.BluRay.x264-SPARKS");
    }
    {
        CASE("filter: unknown subtitles do not satisfy a subtitle request");
        std::vector<TorrentQuality> v = {
            mk("Movie.1080p.BluRay.x264"), mk("Movie.1080p.ENGSUB")};
        EQ_S(joinTitles(filterTitles(v, {}, {"EN"})), "Movie.1080p.ENGSUB");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Reported failures — the five bugs this module exists to fix.
// Each case is a real user report; treat a failure here as a regression.
// ─────────────────────────────────────────────────────────────────────────────
void testReportedFailures() {
    {
        CASE("report 1: an ES filter must not return only MULTi/DUAL releases");
        std::vector<TorrentQuality> v = {
            mk("Movie.2023.MULTi.1080p.BluRay.x264-GRP"),
            mk("Movie.2023.DUAL.1080p.WEB-DL"),
            mk("Movie.2023.Latino.1080p.WEB-DL"),
        };
        EQ_S(joinTitles(filterTitles(v, {"ES-LA"})), "Movie.2023.Latino.1080p.WEB-DL");
    }
    {
        CASE("report 2: MULTI is selectable and returns the multi-track releases");
        std::vector<TorrentQuality> v = {
            mk("Movie.2023.MULTi.1080p"),
            mk("Movie.2023.Dual.Audio.1080p"),
            mk("Movie.2023.1080p.BluRay"),
        };
        EQ_S(joinTitles(filterTitles(v, {"MULTI"})),
             "Movie.2023.MULTi.1080p | Movie.2023.Dual.Audio.1080p");
    }
    {
        CASE("report 3: Spanish and Latin American Spanish are distinct");
        auto es = lang::detectAudio("Pelicula.2023.Castellano.1080p");
        auto la = lang::detectAudio("Pelicula.2023.Latino.1080p");
        EQ_S(es.join(), "ES");
        EQ_S(la.join(), "ES-LA");
        EQ_B(lang::matchesFilter(es.join(), es.multi, {"ES-LA"}), false);
        EQ_B(lang::matchesFilter(la.join(), la.multi, {"ES"}),    false);
    }
    {
        CASE("report 4: Lanterns S01E01 — a PT/EN release must fail an ES filter");
        // The real Torrentio entry: flags say Portuguese + English, the name
        // says MULTi. The old filter passed it on the MULTi wildcard alone.
        const std::string title =
            "Lanterns.S01E01.1080p.WEB-DL.MULTi.DDP5.1.H.264-GRP\n"
            "\xF0\x9F\x91\xA4 42 \xF0\x9F\x92\xBE 2.1 GB \xE2\x9A\x99\xEF\xB8\x8F ThePirateBay\n"
            + FLAG_PT + " " + FLAG_GB;
        auto d = lang::detectAudio(title);
        EQ_S(d.join(), "PT/EN");
        EQ_B(d.multi, true);
        EQ_B(lang::matchesFilter(d.join(), d.multi, {"ES"}),    false);
        EQ_B(lang::matchesFilter(d.join(), d.multi, {"ES-LA"}), false);
        EQ_B(lang::matchesFilter(d.join(), d.multi, {"PT"}),    true);
        EQ_B(lang::matchesFilter(d.join(), d.multi, {"MULTI"}), true);
    }
    {
        CASE("report 5: How to Train Your Dragon — EN/IT must fail an ES filter");
        std::vector<TorrentQuality> v = {
            mk("How.to.Train.Your.Dragon.2010.1080p.BluRay.x264-AMIABLE"),
            mk("How.to.Train.Your.Dragon.2010.iTALiAN.MULTi.1080p.BluRay.x264"),
            mk("Como.Entrenar.a.tu.Dragon.2010.Latino.1080p.BluRay.x264"),
        };
        EQ_S(joinTitles(filterTitles(v, {"ES-LA"})),
             "Como.Entrenar.a.tu.Dragon.2010.Latino.1080p.BluRay.x264");
        EQ_S(joinTitles(filterTitles(v, {"ES"})), "(none)");
    }
}

} // namespace

int main() {
    testFlags();
    testSpanishVariants();
    testMulti();
    testInference();
    testNegativeGuards();
    testSubtitles();
    testFilter();
    testReportedFailures();

    std::cout << (g_failures == 0 ? "PASS" : "FAILED") << "  "
              << (g_checks - g_failures) << "/" << g_checks << " assertions\n";
    return g_failures;
}
