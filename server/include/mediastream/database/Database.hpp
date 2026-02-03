#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace media::database {

    // Content Types
    enum class ContentType {
        MOVIE,
        SERIES,
        ANIME
    };

    // Download Status
    enum class DownloadStatus {
        PENDING,
        DOWNLOADING,
        COMPLETED,
        SEEDING,
        PAUSED,
        ERROR
    };

    // Media Item (Movie/Series/Anime)
    struct MediaItem {
        int id;
        ContentType type;
        std::string title;
        std::string original_title;
        int year;
        std::string description;
        std::string poster_url;
        std::string backdrop_url;
        float rating;
        std::string genres;
        int runtime_minutes;
        std::string tmdb_id;
        std::string imdb_id;
        std::string language;
        int64_t created_at;
        int64_t updated_at;
    };

    // Torrent Information
    struct TorrentInfo {
        int id;
        int media_id;
        std::string info_hash;
        std::string magnet_uri;
        std::string quality;  // 720p, 1080p, 4K, etc.
        int64_t size_bytes;
        int seeders;
        int leechers;
        std::string source;   // YTS, EZTV, Nyaa
        DownloadStatus status;
        float progress;
        std::string file_path;
        int64_t created_at;
        int64_t updated_at;
    };

    // Watch History
    struct WatchHistory {
        int id;
        int media_id;
        int64_t position_seconds;
        int64_t duration_seconds;
        float progress_percent;
        int64_t last_watched;
        bool completed;
    };

    // Series Episode
    struct Episode {
        int id;
        int media_id;  // References parent series
        int season_number;
        int episode_number;
        std::string title;
        std::string description;
        std::string still_url;
        int runtime_minutes;
        std::string air_date;
        int64_t created_at;
    };

    class Database {
    public:
        explicit Database(const std::filesystem::path& db_path);
        ~Database();

        // Initialization
        void initialize();
        void migrate();

        // Media Items
        int insertMediaItem(const MediaItem& item);
        std::optional<MediaItem> getMediaItem(int id);
        std::vector<MediaItem> getAllMedia(ContentType type);
        std::vector<MediaItem> searchMedia(const std::string& query, ContentType type);
        void updateMediaItem(const MediaItem& item);
        void deleteMediaItem(int id);

        // Torrents
        int insertTorrent(const TorrentInfo& torrent);
        std::optional<TorrentInfo> getTorrent(int id);
        std::optional<TorrentInfo> getTorrentByHash(const std::string& info_hash);
        std::vector<TorrentInfo> getTorrentsForMedia(int media_id);
        std::vector<TorrentInfo> getActiveTorrents();
        void updateTorrentStatus(int id, DownloadStatus status, float progress);
        void updateTorrentFilePath(int id, const std::string& file_path);
        void deleteTorrent(int id);

        // Watch History
        void upsertWatchHistory(const WatchHistory& history);
        std::optional<WatchHistory> getWatchHistory(int media_id);
        std::vector<WatchHistory> getRecentlyWatched(int limit = 20);
        void deleteWatchHistory(int media_id);

        // Episodes (for Series)
        int insertEpisode(const Episode& episode);
        std::optional<Episode> getEpisode(int id);
        std::vector<Episode> getEpisodesForSeries(int media_id, int season_number);
        void updateEpisode(const Episode& episode);
        void deleteEpisode(int id);

        // Statistics
        int getMediaCount(ContentType type);
        int64_t getTotalDownloadedSize();
        std::vector<MediaItem> getPopularMedia(ContentType type, int limit = 20);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace media::database

// Made with Bob
