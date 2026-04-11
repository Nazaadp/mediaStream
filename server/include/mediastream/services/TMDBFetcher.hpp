#pragma once

#include "mediastream/services/ContentDiscovery.hpp"
#include <string>
#include <memory>

namespace media::services {

    class TMDBFetcher {
    public:
        TMDBFetcher();
        ~TMDBFetcher();

        // Enriches the given content with TMDB metadata (posters, descriptions, ratings)
        // using the imdb_id or title.
        void enrichContent(DiscoveredContent& content);

        // On-demand season/episode data for series navigation
        std::vector<SeasonInfo>  fetchSeasons(const std::string& imdb_id);
        std::vector<EpisodeInfo> fetchEpisodes(const std::string& imdb_id, int season);
        
    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace media::services
