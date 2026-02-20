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
        
    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace media::services
