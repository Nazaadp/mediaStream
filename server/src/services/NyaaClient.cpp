#include "mediastream/services/ContentDiscovery.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>

using json = nlohmann::json;

namespace media::services {

    // Helper: CURL write callback
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    // Helper: Perform HTTP GET request
    static std::string httpGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "MediaStream/2.0");

            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                spdlog::error("CURL error: {}", curl_easy_strerror(res));
                response.clear();
            }

            curl_easy_cleanup(curl);
        }

        return response;
    }

    // Nyaa Client Implementation (RSS-based)
    class NyaaClient::Impl {
    public:
        const std::string BASE_URL = "https://nyaa.si";

        std::vector<DiscoveredContent> parseRSS(const std::string& rss_response) {
            std::vector<DiscoveredContent> results;

            try {
                // Simple RSS parsing using regex (for production, use proper XML parser)
                std::regex item_regex("<item>([\\s\\S]*?)</item>", std::regex::icase);
                std::regex title_regex("<title>([\\s\\S]*?)</title>");
                std::regex link_regex("<link>([\\s\\S]*?)</link>");
                std::regex seeders_regex("<nyaa:seeders>(\\d+)</nyaa:seeders>");
                std::regex leechers_regex("<nyaa:leechers>(\\d+)</nyaa:leechers>");
                std::regex size_regex("<nyaa:size>([\\s\\S]*?)</nyaa:size>");
                std::regex hash_regex("<nyaa:infoHash>([\\s\\S]*?)</nyaa:infoHash>");

                std::sregex_iterator items_begin(rss_response.begin(), rss_response.end(), item_regex);
                std::sregex_iterator items_end;

                std::map<std::string, DiscoveredContent> anime_map;

                for (std::sregex_iterator i = items_begin; i != items_end; ++i) {
                    std::string item_content = (*i)[1].str();
                    
                    std::smatch title_match, link_match, seeders_match, leechers_match, size_match, hash_match;
                    
                    if (!std::regex_search(item_content, title_match, title_regex)) continue;
                    
                    std::string full_title = title_match[1].str();
                    
                    // Extract anime name (before episode number or quality)
                    std::string anime_name = full_title;
                    size_t bracket_pos = anime_name.find('[');
                    if (bracket_pos != std::string::npos) {
                        anime_name = anime_name.substr(bracket_pos + 1);
                        size_t end_bracket = anime_name.find(']');
                        if (end_bracket != std::string::npos) {
                            anime_name = anime_name.substr(end_bracket + 1);
                        }
                    }
                    
                    // Remove episode numbers and quality tags
                    std::regex clean_regex("\\s*-\\s*\\d+.*|\\s*\\[.*?\\]|\\s*\\(.*?\\)");
                    anime_name = std::regex_replace(anime_name, clean_regex, "");
                    
                    // Trim whitespace
                    anime_name.erase(0, anime_name.find_first_not_of(" \t\n\r"));
                    anime_name.erase(anime_name.find_last_not_of(" \t\n\r") + 1);

                    if (anime_name.empty()) anime_name = full_title;

                    // Create or get existing anime entry
                    if (anime_map.find(anime_name) == anime_map.end()) {
                        DiscoveredContent content;
                        content.title = anime_name;
                        content.original_title = anime_name;
                        content.description = "Anime Series";
                        content.source = "Nyaa";
                        content.language = "ja";
                        content.genres = "Anime";
                        anime_map[anime_name] = content;
                    }

                    // Add torrent info
                    TorrentQuality tq;
                    tq.quality = "1080p"; // Default assumption
                    tq.type = "Nyaa";
                    
                    if (std::regex_search(item_content, seeders_match, seeders_regex)) {
                        tq.seeders = std::stoi(seeders_match[1].str());
                    }
                    
                    if (std::regex_search(item_content, leechers_match, leechers_regex)) {
                        tq.leechers = std::stoi(leechers_match[1].str());
                    }
                    
                    if (std::regex_search(item_content, hash_match, hash_regex)) {
                        tq.hash = hash_match[1].str();
                        
                        // Build magnet URI
                        std::ostringstream magnet;
                        magnet << "magnet:?xt=urn:btih:" << tq.hash
                               << "&dn=" << full_title
                               << "&tr=http://nyaa.tracker.wf:7777/announce"
                               << "&tr=udp://open.stealth.si:80/announce"
                               << "&tr=udp://tracker.opentrackr.org:1337/announce"
                               << "&tr=udp://exodus.desync.com:6969/announce"
                               << "&tr=udp://tracker.torrent.eu.org:451/announce";
                        tq.magnet_uri = magnet.str();
                    }

                    anime_map[anime_name].torrents.push_back(tq);
                }

                // Convert map to vector
                for (auto& [name, content] : anime_map) {
                    results.push_back(content);
                }

            } catch (const std::exception& e) {
                spdlog::error("Nyaa RSS parsing error: {}", e.what());
            }

            return results;
        }
    };

    // Constructor/Destructor
    NyaaClient::NyaaClient() : m_impl(std::make_unique<Impl>()) {
        spdlog::info("Nyaa Client initialized");
    }

    NyaaClient::~NyaaClient() = default;

    // Fetch Popular Anime
    std::vector<DiscoveredContent> NyaaClient::fetchPopular(int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        // Fetch from Nyaa RSS feed (sorted by seeders)
        std::string url = m_impl->BASE_URL + "/?page=rss&f=2&c=1_0&s=seeders&o=desc";

        // https://nyaa.si/?f=2&c=1_0&q=frieren para buscar palabra
        
        spdlog::info("Fetching popular anime from Nyaa...");
        std::string response = httpGet(url);
        
        if (response.empty()) {
            spdlog::error("Failed to fetch from Nyaa");
            return {};
        }

        auto results = m_impl->parseRSS(response);
        
        // Limit results
        if (results.size() > static_cast<size_t>(limit)) {
            results.resize(limit);
        }

        return results;
    }

    // Search Anime
    std::vector<DiscoveredContent> NyaaClient::search(const std::string& query, int limit, int /*page*/, [[maybe_unused]] const std::string& genre, [[maybe_unused]] const std::string& language) {
        // URL encode query (simple version)
        std::string encoded_query = query;
        std::replace(encoded_query.begin(), encoded_query.end(), ' ', '+');
        
        std::string url = m_impl->BASE_URL + "/?page=rss&c=1_2&q=" + encoded_query;
        
        spdlog::info("Searching Nyaa for: {}", query);
        std::string response = httpGet(url);
        
        if (response.empty()) {
            spdlog::error("Failed to search Nyaa");
            return {};
        }

        auto results = m_impl->parseRSS(response);
        
        // Limit results
        if (results.size() > static_cast<size_t>(limit)) {
            results.resize(limit);
        }

        return results;
    }

    // Get Anime by ID
    std::optional<DiscoveredContent> NyaaClient::getById([[maybe_unused]] const std::string& id) {
        spdlog::warn("Nyaa getById not implemented");
        return std::nullopt;
    }

    // Fetch torrents for a specific anime episode by title + episode number.
    // Searches Nyaa RSS for "{title}" then filters by episode pattern in the title.
    std::vector<TorrentQuality> NyaaClient::fetchEpisodeTorrents(
        const std::string& title, int episode)
    {
        if (title.empty()) return {};

        // URL-encode title (spaces → +)
        std::string encoded = title;
        std::replace(encoded.begin(), encoded.end(), ' ', '+');

        // Search Nyaa anime-english (c=1_2) sorted by seeders
        std::string url = m_impl->BASE_URL + "/?page=rss&c=1_2&s=seeders&o=desc&q=" + encoded;
        spdlog::info("Nyaa episode fetch: {} ep {}", title, episode);

        std::string response = httpGet(url);
        if (response.empty()) return {};

        std::vector<TorrentQuality> results;
        try {
            // Build episode filter: matches " - 01 " / "E01" / " 01 [" patterns
            char ep_buf[64];
            std::snprintf(ep_buf, sizeof(ep_buf),
                "(?:[-\\s]%02d[\\s\\[\\(]|[Ee]%02d[^\\d]|\\s%d\\s)", episode, episode, episode);
            std::regex ep_re(ep_buf);

            std::regex item_re("<item>([\\s\\S]*?)</item>", std::regex::icase);
            std::regex title_re("<title>([\\s\\S]*?)</title>");
            std::regex seeders_re("<nyaa:seeders>(\\d+)</nyaa:seeders>");
            std::regex leechers_re("<nyaa:leechers>(\\d+)</nyaa:leechers>");
            std::regex hash_re("<nyaa:infoHash>([\\s\\S]*?)</nyaa:infoHash>");
            std::regex size_re("<nyaa:size>([\\s\\S]*?)</nyaa:size>");

            std::sregex_iterator it(response.begin(), response.end(), item_re);
            std::sregex_iterator end;

            for (; it != end; ++it) {
                std::string item = (*it)[1].str();
                std::smatch m;

                if (!std::regex_search(item, m, title_re)) continue;
                std::string full_title = m[1].str();

                // Filter: must match episode number
                if (!std::regex_search(full_title, ep_re)) continue;

                TorrentQuality tq;
                tq.quality = full_title;
                tq.type = "Nyaa";

                if (std::regex_search(item, m, seeders_re))  tq.seeders  = std::stoi(m[1].str());
                if (std::regex_search(item, m, leechers_re)) tq.leechers = std::stoi(m[1].str());
                if (std::regex_search(item, m, hash_re)) {
                    tq.hash = m[1].str();
                    std::ostringstream magnet;
                    magnet << "magnet:?xt=urn:btih:" << tq.hash
                           << "&dn=" << full_title
                           << "&tr=http://nyaa.tracker.wf:7777/announce"
                           << "&tr=udp://open.stealth.si:80/announce"
                           << "&tr=udp://tracker.opentrackr.org:1337/announce"
                           << "&tr=udp://exodus.desync.com:6969/announce";
                    tq.magnet_uri = magnet.str();
                }
                if (std::regex_search(item, m, size_re)) {
                    // Parse "123.4 MiB" / "1.2 GiB"
                    std::string sz = m[1].str();
                    try {
                        double num = std::stod(sz);
                        if (sz.find("GiB") != std::string::npos || sz.find("GB") != std::string::npos)
                            tq.size_bytes = static_cast<int64_t>(num * 1024 * 1024 * 1024);
                        else
                            tq.size_bytes = static_cast<int64_t>(num * 1024 * 1024);
                    } catch (...) {}
                }
                if (!tq.hash.empty()) results.push_back(tq);
            }
            spdlog::info("Nyaa: {} torrents for \"{}\" ep {}", results.size(), title, episode);
        } catch (const std::exception& e) {
            spdlog::error("Nyaa fetchEpisodeTorrents error: {}", e.what());
        }
        return results;
    }

} // namespace media::services

// Made with Bob
