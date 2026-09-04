#pragma once

// LanguageTags.hpp
// ─────────────────────────────────────────────────────────────────────────────
// Audio / subtitle language detection for torrent release names.
//
// Three signals, in descending confidence:
//
//   1. Flag emoji   Torrentio emits one regional-indicator flag per audio track
//                   it detected (🇬🇧🇪🇸🇲🇽…). Per-track and structured — the only
//                   authoritative language data any of our sources provides.
//   2. Text tokens  Scene / P2P language words in the release name
//                   ("SPANISH", "LATINO", "TRUEFRENCH", "DUBLADO", "ITA").
//   3. Convention   A release with no language marker at all is English
//                   (Japanese for a Nyaa anime release).
//
// The output keeps (1)+(2) — what the name ACTUALLY claims — separate from (3),
// which is only a guess. That separation is the entire point of this file:
// blending them is what made the old filter useless. An unlabelled English rip
// was indistinguishable from a Spanish dub, and "MULTi" was a wildcard that
// matched every language filter, so asking for Spanish returned exactly the
// multi-audio releases that happened to carry Portuguese and Italian.
//
// Codes are uppercase ISO 639-1, with two regional splits that matter to
// viewers and that the sources genuinely distinguish:
//
//   ES     Castilian Spanish (Spain)        🇪🇸  "CASTELLANO"
//   ES-LA  Latin American Spanish           🇲🇽  "LATINO"
//   PT     European Portuguese              🇵🇹  "PORTUGUES"
//   PT-BR  Brazilian Portuguese             🇧🇷  "DUBLADO"
//
// Header-only and std-only, so server/tests/language_tests.cpp builds with a
// bare compiler and no Conan dependencies.
// ─────────────────────────────────────────────────────────────────────────────

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace media::services::lang {

// The pseudo-code a client sends to ask for "multiple audio tracks, whichever
// they are". Never appears in a Detection's code list — it is carried by the
// `multi` flag, so a release can be both MULTI and enumerated (EN/ES-LA).
inline constexpr std::string_view kMultiCode = "MULTI";

// Display value for "we could not tell". Matches the string the API has always
// used for unknown subtitles, so existing clients keep rendering it unchanged.
inline constexpr std::string_view kUnknown = "N/A";

// ─────────────────────────────────────────────────────────────────────────────
// § 1  Detection result
// ─────────────────────────────────────────────────────────────────────────────
struct Detection {
    std::vector<std::string> codes;  // asserted languages, deduped, name order
    bool multi{false};               // release declares multi / dual audio
    bool inferred{false};            // codes are convention, not from the name

    bool empty() const { return codes.empty(); }

    bool has(std::string_view code) const {
        return std::find(codes.begin(), codes.end(), code) != codes.end();
    }

    void add(std::string_view code) {
        if (!code.empty() && !has(code)) codes.emplace_back(code);
    }

    void remove(std::string_view code) {
        codes.erase(std::remove(codes.begin(), codes.end(), code), codes.end());
    }

    // "EN/ES-LA", or "N/A" when nothing was found. The '/' separator is the
    // wire format every client already splits on.
    std::string join() const {
        if (codes.empty()) return std::string(kUnknown);
        std::string out;
        for (const auto& c : codes) {
            if (!out.empty()) out += '/';
            out += c;
        }
        return out;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// § 2  Normalisation
//
// Uppercases ASCII, folds Latin-1 accents to their base letter (ESPAÑOL →
// ESPANOL, PORTUGUÊS → PORTUGUES) so one alias covers every spelling, turns
// every separator into a space, collapses runs, and pads both ends.
//
// The padding is what makes a plain substring search a word-boundary search:
// " ITA " can only match the standalone token, never the "ita" inside
// "Digital". Aliases are therefore written in post-normalisation form —
// "PT-BR" is spelled "PT BR".
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

// Latin-1 Supplement (U+00C0–U+00FF) arrives as the two bytes C3 80..BF.
// Maps the trailing byte to the unaccented uppercase letter; 0 = not a letter.
inline char foldLatin1(unsigned char trail) {
    // Indexed by (trail - 0x80), covering U+00C0..U+00FF.
    static constexpr char kFold[64] = {
        // C0-CF: À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
        'A','A','A','A','A','A','A','C','E','E','E','E','I','I','I','I',
        // D0-DF: Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
        'D','N','O','O','O','O','O',  0,'O','U','U','U','U','Y',  0,  0,
        // E0-EF: à á â ã ä å æ ç è é ê ë ì í î ï
        'A','A','A','A','A','A','A','C','E','E','E','E','I','I','I','I',
        // F0-FF: ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
        'D','N','O','O','O','O','O',  0,'O','U','U','U','U','Y',  0,'Y',
    };
    if (trail < 0x80) return 0;
    return kFold[trail - 0x80];
}

inline bool isSeparator(unsigned char c) {
    return c == ' ' || c == '.' || c == '_' || c == '-' || c == '+' ||
           c == '/' || c == '\\' || c == ',' || c == ';' || c == ':' ||
           c == '|' || c == '(' || c == ')' || c == '[' || c == ']' ||
           c == '{' || c == '}' || c == '<' || c == '>' || c == '"' ||
           c == '\'' || c == '\t' || c == '\n' || c == '\r' || c == '&' ||
           c == '!' || c == '?' || c == '*' || c == '=' || c == '~' ||
           c == '@' || c == '#' || c == '$' || c == '%' || c == '^';
}

} // namespace detail

inline std::string normalize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    out += ' ';
    bool prev_space = true;

    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);

        // Two-byte Latin-1 Supplement → folded ASCII letter.
        if (c == 0xC3 && i + 1 < raw.size()) {
            const char folded =
                detail::foldLatin1(static_cast<unsigned char>(raw[i + 1]));
            if (folded != 0) {
                out += folded;
                prev_space = false;
                ++i;
                continue;
            }
        }

        // Any other non-ASCII byte (emoji, CJK, …) is a separator here: § 3
        // reads the flags straight off the raw string, before normalisation.
        if (c >= 0x80 || detail::isSeparator(c)) {
            if (!prev_space) { out += ' '; prev_space = true; }
            continue;
        }

        out += static_cast<char>(std::toupper(c));
        prev_space = false;
    }

    if (out.empty() || out.back() != ' ') out += ' ';
    return out;
}

// Word-boundary containment. `normalized` must come from normalize(); `token`
// must already be in normalised (uppercase, space-separated) form.
inline bool hasToken(const std::string& normalized, std::string_view token) {
    std::string padded;
    padded.reserve(token.size() + 2);
    padded += ' ';
    padded += token;
    padded += ' ';
    return normalized.find(padded) != std::string::npos;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 3  Flag emoji decoding
//
// A flag is two regional-indicator symbols, U+1F1E6..U+1F1FF, each encoded as
// the four bytes F0 9F 87 A6..BF — so the trailing byte alone gives the letter.
// Decoding the pair generically beats a table of literal flag strings: it costs
// one branch and covers every country any source might emit.
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

inline bool regionalIndicator(const std::string& s, size_t i, char& letter) {
    if (i + 3 >= s.size()) return false;
    if (static_cast<unsigned char>(s[i])     != 0xF0) return false;
    if (static_cast<unsigned char>(s[i + 1]) != 0x9F) return false;
    if (static_cast<unsigned char>(s[i + 2]) != 0x87) return false;
    const unsigned char t = static_cast<unsigned char>(s[i + 3]);
    if (t < 0xA6 || t > 0xBF) return false;
    letter = static_cast<char>('A' + (t - 0xA6));
    return true;
}

// Country (ISO 3166-1 alpha-2) → language code. Only entries that actually
// signal an audio language; an unmapped flag is ignored rather than guessed,
// so an unknown country can never fabricate a match.
inline std::string_view countryToLang(char c0, char c1) {
    struct Row { char cc[2]; const char* lang; };
    static constexpr Row kRows[] = {
        // English
        {{'G','B'}, "EN"}, {{'U','S'}, "EN"}, {{'A','U'}, "EN"},
        {{'C','A'}, "EN"}, {{'I','E'}, "EN"}, {{'N','Z'}, "EN"},
        // Spanish — the split that matters. Torrentio uses 🇲🇽 for "latino";
        // the rest of Latin America is here for other addons and for hand-made
        // release names that use their own country's flag.
        {{'E','S'}, "ES"},
        {{'M','X'}, "ES-LA"}, {{'A','R'}, "ES-LA"}, {{'C','L'}, "ES-LA"},
        {{'C','O'}, "ES-LA"}, {{'P','E'}, "ES-LA"}, {{'V','E'}, "ES-LA"},
        {{'U','Y'}, "ES-LA"}, {{'P','Y'}, "ES-LA"}, {{'B','O'}, "ES-LA"},
        {{'E','C'}, "ES-LA"}, {{'C','R'}, "ES-LA"}, {{'D','O'}, "ES-LA"},
        {{'G','T'}, "ES-LA"}, {{'H','N'}, "ES-LA"}, {{'N','I'}, "ES-LA"},
        {{'P','A'}, "ES-LA"}, {{'S','V'}, "ES-LA"}, {{'C','U'}, "ES-LA"},
        {{'P','R'}, "ES-LA"},
        // Portuguese
        {{'P','T'}, "PT"}, {{'B','R'}, "PT-BR"},
        // Everything else
        {{'F','R'}, "FR"}, {{'D','E'}, "DE"}, {{'A','T'}, "DE"},
        {{'I','T'}, "IT"}, {{'J','P'}, "JA"}, {{'K','R'}, "KO"},
        {{'C','N'}, "ZH"}, {{'T','W'}, "ZH"}, {{'H','K'}, "ZH"},
        {{'R','U'}, "RU"}, {{'I','N'}, "HI"}, {{'N','L'}, "NL"},
        {{'B','E'}, "NL"}, {{'P','L'}, "PL"}, {{'T','R'}, "TR"},
        {{'S','A'}, "AR"}, {{'A','E'}, "AR"}, {{'E','G'}, "AR"},
        {{'I','R'}, "FA"}, {{'I','L'}, "HE"}, {{'V','N'}, "VI"},
        {{'I','D'}, "ID"}, {{'M','Y'}, "MS"}, {{'T','H'}, "TH"},
        {{'G','R'}, "EL"}, {{'U','A'}, "UK"}, {{'C','Z'}, "CS"},
        {{'S','K'}, "SK"}, {{'S','I'}, "SL"}, {{'H','U'}, "HU"},
        {{'R','O'}, "RO"}, {{'B','G'}, "BG"}, {{'R','S'}, "SR"},
        {{'H','R'}, "HR"}, {{'D','K'}, "DA"}, {{'F','I'}, "FI"},
        {{'S','E'}, "SV"}, {{'N','O'}, "NO"}, {{'L','T'}, "LT"},
        {{'L','V'}, "LV"}, {{'E','E'}, "ET"},
    };
    for (const auto& r : kRows) {
        if (c0 == r.cc[0] && c1 == r.cc[1]) return r.lang;
    }
    return {};
}

} // namespace detail

// Appends every language a flag emoji in `raw` names, in order of appearance.
inline void collectFlagLangs(const std::string& raw, Detection& out) {
    for (size_t i = 0; i + 7 < raw.size();) {
        char a = 0, b = 0;
        if (detail::regionalIndicator(raw, i, a) &&
            detail::regionalIndicator(raw, i + 4, b)) {
            out.add(detail::countryToLang(a, b));
            i += 8;
        } else {
            ++i;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// § 3b  Flag lines are channel-tagged
//
// Torrentio puts its flags on the last line of the title block, and prefixes
// that line to say WHICH channel they describe. Observed forms:
//
//   Multi Audio / 🇬🇧 / 🇫🇷          ← audio tracks
//   Multi Subs / 🇬🇧 / 🇮🇹           ← SUBTITLE tracks
//   Dual Audio                        ← audio, count only, no languages
//   🇬🇧 / 🇮🇹                        ← bare: audio
//
// Reading every flag as audio would make a multi-SUBTITLE release answer an
// audio request for those languages — the same false positive this module
// exists to kill, just arriving through a different door. So flags are
// collected per line and routed by that line's prefix.
// ─────────────────────────────────────────────────────────────────────────────
struct FlagLines {
    Detection audio;
    Detection subs;
};

inline FlagLines collectFlagLines(const std::string& raw) {
    FlagLines out;
    size_t start = 0;
    for (;;) {
        const size_t nl  = raw.find('\n', start);
        const size_t end = (nl == std::string::npos) ? raw.size() : nl;
        const std::string line = raw.substr(start, end - start);

        Detection here;
        collectFlagLangs(line, here);
        if (!here.codes.empty()) {
            const std::string n = normalize(line);
            const bool is_subs = hasToken(n, "SUB")   || hasToken(n, "SUBS") ||
                                 hasToken(n, "SUBTITLE") ||
                                 hasToken(n, "SUBTITLES") ||
                                 hasToken(n, "LEGENDAS");
            Detection& target = is_subs ? out.subs : out.audio;
            for (const auto& c : here.codes) target.add(c);
        }

        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 4  Text token vocabulary
//
// `weak` aliases are short or ambiguous enough to false-positive on their own
// ("CAST" in "Cast Away", "LAT" in a group name), so they only assert their
// language once some other language signal has already been found. Strong
// aliases run first for exactly that reason.
//
// Deliberately absent: bare "ES", "IT", "DE", "RU", "PL", "RO". They are real
// English words or title words ("It", "De Palma"), and any release that means
// them as a language spells the tag out or uses the 3-letter form.
// ─────────────────────────────────────────────────────────────────────────────
struct Alias {
    std::string_view code;
    std::string_view token;   // normalised form
    bool             weak;
};

inline const std::vector<Alias>& audioAliases() {
    static const std::vector<Alias> kTable = {
        // ── English ──────────────────────────────────────────────────────────
        {"EN", "ENGLISH", false},
        {"EN", "ENG",     false},
        // ── Spanish: Latin American ──────────────────────────────────────────
        {"ES-LA", "LATINO",          false},
        {"ES-LA", "LATINOAMERICANO", false},
        {"ES-LA", "ESPANOL LATINO",  false},
        {"ES-LA", "AUDIO LATINO",    false},
        {"ES-LA", "DUAL LATINO",     false},
        {"ES-LA", "LATAM",           false},
        {"ES-LA", "ES 419",          false},   // BCP-47 es-419
        {"ES-LA", "ES LA",           false},
        {"ES-LA", "SPANISH LATIN",   false},
        {"ES-LA", "LATIN SPANISH",   false},
        {"ES-LA", "MEXICAN",         false},
        {"ES-LA", "LAT",             true},
        // ── Spanish: Castilian ───────────────────────────────────────────────
        // CASTELLANO / ES ES / CAST are explicit Spain markers: they survive
        // the regional-parent rule in § 5 even when LATINO is also present,
        // because a "Castellano + Latino" dual release really carries both.
        {"ES", "CASTELLANO", false},
        {"ES", "ES ES",      false},
        {"ES", "SPANISH",    false},
        {"ES", "ESPANOL",    false},
        {"ES", "SPA",        false},
        {"ES", "ESP",        false},
        {"ES", "CAST",       true},
        // ── Portuguese ───────────────────────────────────────────────────────
        {"PT-BR", "DUBLADO",      false},
        {"PT-BR", "PT BR",        false},
        {"PT-BR", "PTBR",         false},
        {"PT-BR", "BRAZILIAN",    false},
        {"PT-BR", "PORTUGUES BR", false},
        {"PT-BR", "NACIONAL",     true},
        {"PT", "PORTUGUESE", false},
        {"PT", "PORTUGUES",  false},
        {"PT", "PT PT",      false},
        {"PT", "POR",        true},
        // ── French ───────────────────────────────────────────────────────────
        // VOSTFR is French SUBTITLES over the original audio — deliberately
        // absent here; § 6 reads it as a subtitle tag.
        {"FR", "FRENCH",     false},
        {"FR", "TRUEFRENCH", false},
        {"FR", "FRANCAIS",   false},
        {"FR", "VFF",        false},
        {"FR", "VFQ",        false},
        {"FR", "VFI",        false},
        {"FR", "VFB",        false},
        {"FR", "VFHQ",       false},
        {"FR", "FRE",        true},
        {"FR", "FRA",        true},
        {"FR", "VF",         true},
        // ── Rest of the world ────────────────────────────────────────────────
        {"DE", "GERMAN",     false}, {"DE", "DEUTSCH",    false},
        {"DE", "GER",        true},
        {"IT", "ITALIAN",    false}, {"IT", "ITALIANO",   false},
        {"IT", "ITA",        false},
        {"JA", "JAPANESE",   false}, {"JA", "JPN",        false},
        {"JA", "JAP",        true},
        {"KO", "KOREAN",     false}, {"KO", "KOR",        true},
        {"ZH", "CHINESE",    false}, {"ZH", "MANDARIN",   false},
        {"ZH", "CANTONESE",  false}, {"ZH", "CHI",        true},
        {"RU", "RUSSIAN",    false}, {"RU", "RUS",        true},
        {"HI", "HINDI",      false}, {"HI", "HIN",        true},
        {"TA", "TAMIL",      false}, {"TE", "TELUGU",     false},
        {"AR", "ARABIC",     false}, {"AR", "ARA",        true},
        {"TR", "TURKISH",    false}, {"TR", "TUR",        true},
        {"NL", "DUTCH",      false}, {"NL", "NEDERLANDS", false},
        {"PL", "POLISH",     false}, {"PL", "PLDUB",      false},
        {"PL", "POL",        true},
        {"CS", "CZECH",      false}, {"SK", "SLOVAK",     false},
        {"HU", "HUNGARIAN",  false}, {"RO", "ROMANIAN",   false},
        {"BG", "BULGARIAN",  false}, {"SR", "SERBIAN",    false},
        {"HR", "CROATIAN",   false}, {"UK", "UKRAINIAN",  false},
        {"EL", "GREEK",      false}, {"DA", "DANISH",     false},
        {"FI", "FINNISH",    false}, {"SV", "SWEDISH",    false},
        {"NO", "NORWEGIAN",  false}, {"TH", "THAI",       false},
        {"VI", "VIETNAMESE", false}, {"ID", "INDONESIAN", false},
        {"MS", "MALAY",      false}, {"HE", "HEBREW",     false},
        {"FA", "PERSIAN",    false},
    };
    return kTable;
}

// "Multi Subs" / "Multi-Sub" declares SUBTITLE tracks. Blank those phrases out
// before testing for a multi-AUDIO marker, or the bare "MULTI" inside them
// makes every multi-subtitle release answer a multi-audio request — Torrentio
// emits "Multi Subs / 🇬🇧 / 🇮🇹" lines, so this is not hypothetical.
inline std::string maskSubtitleMulti(std::string n) {
    static constexpr std::string_view kPhrases[] = {
        "MULTI SUBS", "MULTI SUB", "MULTISUBS", "MULTISUB",
        "MULTI SUBTITLES", "MULTI SUBTITLE", "MULTISUBTITLES", "MULTISUBTITLE",
    };
    for (auto p : kPhrases) {
        const std::string needle = " " + std::string(p) + " ";
        for (size_t pos = n.find(needle); pos != std::string::npos;
             pos = n.find(needle, pos + 1)) {
            n.replace(pos + 1, p.size(), std::string(p.size(), ' '));
        }
    }
    return n;
}

// "This release has more than one audio track" — without saying which.
// Callers pass a string already run through maskSubtitleMulti().
inline bool hasMultiMarker(const std::string& n) {
    static constexpr std::string_view kTokens[] = {
        "MULTI", "MULTI AUDIO", "MULTIAUDIO", "MULTIAUDIOS", "MULTILANG",
        "MULTILANGUAGE", "MULTILINGUAL", "MULTI LANG", "MULTI LANGUAGE",
        "MULTI 3", "MULTI 5", "MULTI 7", "MULTI 9", "MULTI3", "MULTI5",
        "DUAL", "DUAL AUDIO", "DUALAUDIO", "DUAL LATINO",
        "TRIAUDIO", "TRI AUDIO", "TRIPLE AUDIO",
    };
    for (auto t : kTokens) if (hasToken(n, t)) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 5  Audio detection
//
// `default_code` is the convention fallback: "EN" for scene/P2P sources, "JA"
// for Nyaa (an English-subbed anime release is Japanese audio). It is applied
// ONLY when the name carries no language signal whatsoever — never when the
// name says MULTi, because "some unknown set of languages" must not silently
// become "English" and sail through an English filter.
// ─────────────────────────────────────────────────────────────────────────────
inline Detection detectAudioWithDefault(const std::string& raw,
                                        std::string_view default_code) {
    Detection d;

    // 1. Flags — authoritative, so they go in first and set the display order.
    //    Only the ones on an AUDIO-tagged line (§ 3b); a "Multi Subs" line's
    //    flags belong to detectSubs. Kept in their own Detection as well:
    //    step 4 may demote a text-derived code, but never a flagged one.
    const Detection flags = collectFlagLines(raw).audio;
    for (const auto& c : flags.codes) d.add(c);

    const std::string n = normalize(raw);

    // 2. Multi / dual marker, with the subtitle-multi phrases masked out.
    d.multi = hasMultiMarker(maskSubtitleMulti(n));

    // 3. Strong text tokens, then weak ones once something has corroborated.
    const auto& table = audioAliases();
    for (const auto& a : table) {
        if (!a.weak && hasToken(n, a.token)) d.add(a.code);
    }
    if (!d.codes.empty() || d.multi) {
        for (const auto& a : table) {
            if (a.weak && hasToken(n, a.token)) d.add(a.code);
        }
    }

    // 4. Regional-parent resolution. "Spanish Latino" asserts Latin American
    //    Spanish once, not Castilian as well — the generic parent tag is just
    //    how that name spells the language. Two things override this and keep
    //    both codes, because both are real dual releases:
    //      • an explicit parent marker ("CASTELLANO", "ES ES", "PT PT")
    //      • BOTH codes came from flags. 🇪🇸 alongside 🇲🇽 is two separately
    //        declared audio tracks. One flag plus a text tag is not: Torrentio
    //        maps all Portuguese to 🇵🇹, so a Brazilian dub arrives as
    //        "🇵🇹 + Dublado" — one track the text refines, and reporting PT
    //        as well would over-claim a European track that isn't there.
    struct ParentRule {
        std::string_view child;
        std::string_view parent;
        std::string_view explicit_parent[3];
    };
    static constexpr ParentRule kRules[] = {
        {"ES-LA", "ES", {"CASTELLANO", "ES ES", "CAST"}},
        {"PT-BR", "PT", {"PT PT", "PORTUGAL", "LUSO"}},
    };
    for (const auto& r : kRules) {
        if (!d.has(r.child) || !d.has(r.parent)) continue;
        if (flags.has(r.parent) && flags.has(r.child)) continue;
        bool keep_parent = false;
        for (auto t : r.explicit_parent) {
            if (!t.empty() && hasToken(n, t)) { keep_parent = true; break; }
        }
        if (!keep_parent) d.remove(r.parent);
    }

    // 5. Convention fallback.
    if (d.codes.empty() && !d.multi && !default_code.empty()) {
        d.add(default_code);
        d.inferred = true;
    }

    return d;
}

// Scene / P2P sources (Torrentio, EZTV, YTS).
inline Detection detectAudio(const std::string& raw) {
    return detectAudioWithDefault(raw, "EN");
}

// Nyaa: the English-translated anime category is Japanese audio unless the
// name says otherwise, and "Dual Audio" there means Japanese + English.
inline Detection detectAnimeAudio(const std::string& raw) {
    Detection d = detectAudioWithDefault(raw, "JA");
    if (d.multi && d.codes.empty()) {
        // The one case where MULTI has a known meaning — fansub convention.
        d.add("JA");
        d.add("EN");
        d.inferred = true;
    }
    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 6  Subtitle detection
//
// Subtitle tags are rarer and noisier than audio tags, so this stays strictly
// evidence-based: no convention fallback, and a streaming-service tag sets
// only `multi` (an AMZN/NF WEB-DL really does ship many subtitle tracks, but
// the name never says which) instead of inventing per-language claims that a
// subtitle filter would then happily match.
// ─────────────────────────────────────────────────────────────────────────────
inline Detection detectSubs(const std::string& raw) {
    Detection d;
    const std::string n = normalize(raw);

    // 0. Flags from a SUBTITLE-tagged line (§ 3b) — Torrentio's "Multi Subs /
    //    🇬🇧 / 🇮🇹". The one place subtitle languages are stated exactly.
    for (const auto& c : collectFlagLines(raw).subs.codes) d.add(c);

    // 1. Glued / idiomatic subtitle tags.
    struct SubTag { std::string_view token, code; };
    static constexpr SubTag kTags[] = {
        {"VOSTFR",              "FR"},   // VO sous-titrée français
        {"SUBFRENCH",           "FR"},
        {"STFR",                "FR"},
        {"VOSE",                "ES"},   // VO subtitulada en español
        {"VOSES",               "ES"},
        {"SUBTITULADO",         "ES"},
        {"SUBS ESPANOL",        "ES"},
        {"SUBTITULOS ESPANOL",  "ES"},
        {"LEGENDADO",           "PT-BR"},
        {"LEGENDA",             "PT-BR"},
        {"LEGENDAS",            "PT-BR"},
        {"ENGSUB",              "EN"},
        {"ENGSUBS",             "EN"},
        {"ESUB",                "EN"},
        {"ESUBS",               "EN"},
        {"SUBBED",              "EN"},
        {"NLSUBS",              "NL"},
        {"ITASUB",              "IT"},
        {"GERSUB",              "DE"},
    };
    for (const auto& t : kTags) {
        if (hasToken(n, t.token)) d.add(t.code);
    }

    // 2. Multi-subtitle markers.
    static constexpr std::string_view kMultiSub[] = {
        "MULTISUB", "MULTI SUB", "MULTI SUBS", "MULTISUBS", "MSUB", "MSUBS",
        "MULTI SUBTITLE", "MULTI SUBTITLES", "MULTISUBTITLE",
    };
    for (auto t : kMultiSub) {
        if (hasToken(n, t)) { d.multi = true; break; }
    }

    // 3. Language words sitting in a subtitle context window. Catches the
    //    spaced forms ("Subs: English, Spanish", "Spanish Subtitles") the glued
    //    table can't enumerate, while keeping the audio vocabulary as the one
    //    source of truth for what each word means.
    //
    //    The window is asymmetric on purpose: languages come immediately
    //    before the keyword ("Spanish Subs") or in a list after it ("Subs: EN,
    //    ES, FR"). Reaching further back would swallow the audio tags in
    //    "SPANISH.AUDIO.ENGLISH.SUBS" and claim Spanish subtitles too.
    constexpr size_t kBefore = 16, kAfter = 40;
    static constexpr std::string_view kSubWords[] = {
        "SUB", "SUBS", "SUBTITLE", "SUBTITLES", "SUBTITULOS", "SOUS TITRES",
    };
    for (auto w : kSubWords) {
        const std::string needle = " " + std::string(w) + " ";
        for (size_t pos = n.find(needle); pos != std::string::npos;
             pos = n.find(needle, pos + 1)) {
            const size_t lo = (pos > kBefore) ? pos - kBefore : 0;
            const size_t hi = std::min(n.size(), pos + needle.size() + kAfter);
            const std::string window = " " + n.substr(lo, hi - lo) + " ";
            for (const auto& a : audioAliases()) {
                if (!a.weak && hasToken(window, a.token)) d.add(a.code);
            }
        }
    }

    // 4. Streaming-service WEB-DLs carry many subtitle tracks, but the name
    //    never lists them. `multi` only — a soft signal the user can opt into
    //    with the MULTI chip, and which no per-language filter will match.
    //    "MAX" and "STAN" are absent: they are "Mad Max" and "Stan & Ollie"
    //    far more often than they are a streaming service.
    static constexpr std::string_view kServices[] = {
        "NF", "AMZN", "DSNP", "HMAX", "ATVP", "PCOK", "HULU", "CRAV", "SKST",
    };
    if (d.codes.empty()) {
        for (auto s : kServices) {
            if (hasToken(n, s)) { d.multi = true; break; }
        }
    }

    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
// § 7  Filter matching
//
// `wanted` is the uppercase set from ?audio= / ?subs=, and may contain the
// MULTI pseudo-code. The rule is deliberately strict: a torrent passes only on
// evidence.
//
//   • no filter                         → pass
//   • MULTI requested, release is multi → pass
//   • an asserted code is wanted        → pass
//   • anything else                     → drop
//
// What is NOT here is the old escape hatch that let MULTI / DUAL / N/A pass
// every filter. That one line is why asking for Spanish returned Portuguese
// and Italian: real Spanish rips were mislabelled "EN" by the old default and
// dropped, while every multi-audio release matched unconditionally, so the
// only survivors were the multi-audio ones. Unknown now means unknown — a
// release the name never labelled cannot satisfy a language request.
//
// Convention-inferred codes do take part: an untagged scene rip is labelled EN
// and passes an English filter, the one guess right often enough to keep.
// `inferred` rides along on the wire so the client can mark it as a guess.
// ─────────────────────────────────────────────────────────────────────────────
inline bool matchesFilter(const std::string& joined_codes, bool multi,
                          const std::unordered_set<std::string>& wanted) {
    if (wanted.empty()) return true;
    if (multi && wanted.count(std::string(kMultiCode))) return true;
    if (joined_codes.empty() || joined_codes == kUnknown) return false;

    size_t start = 0;
    for (;;) {
        const size_t sep = joined_codes.find('/', start);
        const size_t end = (sep == std::string::npos) ? joined_codes.size() : sep;
        if (wanted.count(joined_codes.substr(start, end - start))) return true;
        if (sep == std::string::npos) return false;
        start = sep + 1;
    }
}

} // namespace media::services::lang
