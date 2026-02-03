#include "mediastream/database/Database.hpp"
#include <oatpp-sqlite/orm.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace media::database {

    // Helper: Convert enum to string
    std::string contentTypeToString(ContentType type) {
        switch(type) {
            case ContentType::MOVIE: return "MOVIE";
            case ContentType::SERIES: return "SERIES";
            case ContentType::ANIME: return "ANIME";
            default: return "MOVIE";
        }
    }

    ContentType stringToContentType(const std::string& str) {
        if (str == "SERIES") return ContentType::SERIES;
        if (str == "ANIME") return ContentType::ANIME;
        return ContentType::MOVIE;
    }

    std::string downloadStatusToString(DownloadStatus status) {
        switch(status) {
            case DownloadStatus::PENDING: return "PENDING";
            case DownloadStatus::DOWNLOADING: return "DOWNLOADING";
            case DownloadStatus::COMPLETED: return "COMPLETED";
            case DownloadStatus::SEEDING: return "SEEDING";
            case DownloadStatus::PAUSED: return "PAUSED";
            case DownloadStatus::ERROR: return "ERROR";
            default: return "PENDING";
        }
    }

    DownloadStatus stringToDownloadStatus(const std::string& str) {
        if (str == "DOWNLOADING") return DownloadStatus::DOWNLOADING;
        if (str == "COMPLETED") return DownloadStatus::COMPLETED;
        if (str == "SEEDING") return DownloadStatus::SEEDING;
        if (str == "PAUSED") return DownloadStatus::PAUSED;
        if (str == "ERROR") return DownloadStatus::ERROR;
        return DownloadStatus::PENDING;
    }

    // PIMPL Implementation
    class Database::Impl {
    public:
        std::shared_ptr<oatpp::sqlite::Executor> executor;
        
        explicit Impl(const std::filesystem::path& db_path) {
            auto connectionProvider = std::make_shared<oatpp::sqlite::ConnectionProvider>(db_path.string());
            executor = std::make_shared<oatpp::sqlite::Executor>(connectionProvider);
            spdlog::info("Database initialized at: {}", db_path.string());
        }

        void executeSQL(const std::string& sql) {
            auto result = executor->execute(sql, {});
            if (!result->isSuccess()) {
                throw std::runtime_error("SQL execution failed: " + sql);
            }
        }
    };

    // Constructor
    Database::Database(const std::filesystem::path& db_path) 
        : m_impl(std::make_unique<Impl>(db_path)) {
    }

    Database::~Database() = default;

    // Initialize database schema
    void Database::initialize() {
        spdlog::info("Creating database schema...");

        // Media Items Table
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS media_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                type TEXT NOT NULL,
                title TEXT NOT NULL,
                original_title TEXT,
                year INTEGER,
                description TEXT,
                poster_url TEXT,
                backdrop_url TEXT,
                rating REAL DEFAULT 0.0,
                genres TEXT,
                runtime_minutes INTEGER,
                tmdb_id TEXT,
                imdb_id TEXT,
                language TEXT DEFAULT 'en',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            )
        )");

        // Torrents Table
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS torrents (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                info_hash TEXT UNIQUE NOT NULL,
                magnet_uri TEXT NOT NULL,
                quality TEXT,
                size_bytes INTEGER DEFAULT 0,
                seeders INTEGER DEFAULT 0,
                leechers INTEGER DEFAULT 0,
                source TEXT,
                status TEXT DEFAULT 'PENDING',
                progress REAL DEFAULT 0.0,
                file_path TEXT,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE
            )
        )");

        // Watch History Table
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS watch_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                position_seconds INTEGER DEFAULT 0,
                duration_seconds INTEGER DEFAULT 0,
                progress_percent REAL DEFAULT 0.0,
                last_watched INTEGER NOT NULL,
                completed INTEGER DEFAULT 0,
                FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE,
                UNIQUE(media_id)
            )
        )");

        // Episodes Table (for Series/Anime)
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS episodes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                season_number INTEGER NOT NULL,
                episode_number INTEGER NOT NULL,
                title TEXT,
                description TEXT,
                still_url TEXT,
                runtime_minutes INTEGER,
                air_date TEXT,
                created_at INTEGER NOT NULL,
                FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE,
                UNIQUE(media_id, season_number, episode_number)
            )
        )");

        // Create indexes for performance
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_media_type ON media_items(type)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_media_title ON media_items(title)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_hash ON torrents(info_hash)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_media ON torrents(media_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_status ON torrents(status)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_watch_history_media ON watch_history(media_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_episodes_media ON episodes(media_id)");

        spdlog::info("Database schema created successfully");
    }

    void Database::migrate() {
        // Future migrations will go here
        spdlog::info("Database migrations completed");
    }

    // Media Items
    int Database::insertMediaItem(const MediaItem& item) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->executor->execute(
            "INSERT INTO media_items (type, title, original_title, year, description, poster_url, "
            "backdrop_url, rating, genres, runtime_minutes, tmdb_id, imdb_id, language, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            {
                contentTypeToString(item.type),
                item.title,
                item.original_title,
                std::to_string(item.year),
                item.description,
                item.poster_url,
                item.backdrop_url,
                std::to_string(item.rating),
                item.genres,
                std::to_string(item.runtime_minutes),
                item.tmdb_id,
                item.imdb_id,
                item.language,
                std::to_string(now),
                std::to_string(now)
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert media item");
    }

    std::optional<MediaItem> Database::getMediaItem(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM media_items WHERE id = ?",
            {std::to_string(id)}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            MediaItem item;
            item.id = row->getInt32(0);
            item.type = stringToContentType(row->getString(1));
            item.title = row->getString(2);
            item.original_title = row->getString(3);
            item.year = row->getInt32(4);
            item.description = row->getString(5);
            item.poster_url = row->getString(6);
            item.backdrop_url = row->getString(7);
            item.rating = row->getFloat32(8);
            item.genres = row->getString(9);
            item.runtime_minutes = row->getInt32(10);
            item.tmdb_id = row->getString(11);
            item.imdb_id = row->getString(12);
            item.language = row->getString(13);
            item.created_at = row->getInt64(14);
            item.updated_at = row->getInt64(15);
            return item;
        }
        return std::nullopt;
    }

    std::vector<MediaItem> Database::getAllMedia(ContentType type) {
        std::vector<MediaItem> items;
        auto result = m_impl->executor->execute(
            "SELECT * FROM media_items WHERE type = ? ORDER BY created_at DESC",
            {contentTypeToString(type)}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                MediaItem item;
                item.id = row->getInt32(0);
                item.type = stringToContentType(row->getString(1));
                item.title = row->getString(2);
                item.original_title = row->getString(3);
                item.year = row->getInt32(4);
                item.description = row->getString(5);
                item.poster_url = row->getString(6);
                item.backdrop_url = row->getString(7);
                item.rating = row->getFloat32(8);
                item.genres = row->getString(9);
                item.runtime_minutes = row->getInt32(10);
                item.tmdb_id = row->getString(11);
                item.imdb_id = row->getString(12);
                item.language = row->getString(13);
                item.created_at = row->getInt64(14);
                item.updated_at = row->getInt64(15);
                items.push_back(item);
            }
        }
        return items;
    }

    std::vector<MediaItem> Database::searchMedia(const std::string& query, ContentType type) {
        std::vector<MediaItem> items;
        auto result = m_impl->executor->execute(
            "SELECT * FROM media_items WHERE type = ? AND (title LIKE ? OR original_title LIKE ?) ORDER BY rating DESC",
            {contentTypeToString(type), "%" + query + "%", "%" + query + "%"}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                MediaItem item;
                item.id = row->getInt32(0);
                item.type = stringToContentType(row->getString(1));
                item.title = row->getString(2);
                item.original_title = row->getString(3);
                item.year = row->getInt32(4);
                item.description = row->getString(5);
                item.poster_url = row->getString(6);
                item.backdrop_url = row->getString(7);
                item.rating = row->getFloat32(8);
                item.genres = row->getString(9);
                item.runtime_minutes = row->getInt32(10);
                item.tmdb_id = row->getString(11);
                item.imdb_id = row->getString(12);
                item.language = row->getString(13);
                item.created_at = row->getInt64(14);
                item.updated_at = row->getInt64(15);
                items.push_back(item);
            }
        }
        return items;
    }

    void Database::updateMediaItem(const MediaItem& item) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "UPDATE media_items SET title = ?, original_title = ?, year = ?, description = ?, "
            "poster_url = ?, backdrop_url = ?, rating = ?, genres = ?, runtime_minutes = ?, "
            "tmdb_id = ?, imdb_id = ?, language = ?, updated_at = ? WHERE id = ?",
            {
                item.title,
                item.original_title,
                std::to_string(item.year),
                item.description,
                item.poster_url,
                item.backdrop_url,
                std::to_string(item.rating),
                item.genres,
                std::to_string(item.runtime_minutes),
                item.tmdb_id,
                item.imdb_id,
                item.language,
                std::to_string(now),
                std::to_string(item.id)
            }
        );
    }

    void Database::deleteMediaItem(int id) {
        m_impl->executor->execute("DELETE FROM media_items WHERE id = ?", {std::to_string(id)});
    }

    // Torrents
    int Database::insertTorrent(const TorrentInfo& torrent) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->executor->execute(
            "INSERT INTO torrents (media_id, info_hash, magnet_uri, quality, size_bytes, seeders, "
            "leechers, source, status, progress, file_path, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            {
                std::to_string(torrent.media_id),
                torrent.info_hash,
                torrent.magnet_uri,
                torrent.quality,
                std::to_string(torrent.size_bytes),
                std::to_string(torrent.seeders),
                std::to_string(torrent.leechers),
                torrent.source,
                downloadStatusToString(torrent.status),
                std::to_string(torrent.progress),
                torrent.file_path,
                std::to_string(now),
                std::to_string(now)
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert torrent");
    }

    std::optional<TorrentInfo> Database::getTorrent(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM torrents WHERE id = ?",
            {std::to_string(id)}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            TorrentInfo torrent;
            torrent.id = row->getInt32(0);
            torrent.media_id = row->getInt32(1);
            torrent.info_hash = row->getString(2);
            torrent.magnet_uri = row->getString(3);
            torrent.quality = row->getString(4);
            torrent.size_bytes = row->getInt64(5);
            torrent.seeders = row->getInt32(6);
            torrent.leechers = row->getInt32(7);
            torrent.source = row->getString(8);
            torrent.status = stringToDownloadStatus(row->getString(9));
            torrent.progress = row->getFloat32(10);
            torrent.file_path = row->getString(11);
            torrent.created_at = row->getInt64(12);
            torrent.updated_at = row->getInt64(13);
            return torrent;
        }
        return std::nullopt;
    }

    std::optional<TorrentInfo> Database::getTorrentByHash(const std::string& info_hash) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM torrents WHERE info_hash = ?",
            {info_hash}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            TorrentInfo torrent;
            torrent.id = row->getInt32(0);
            torrent.media_id = row->getInt32(1);
            torrent.info_hash = row->getString(2);
            torrent.magnet_uri = row->getString(3);
            torrent.quality = row->getString(4);
            torrent.size_bytes = row->getInt64(5);
            torrent.seeders = row->getInt32(6);
            torrent.leechers = row->getInt32(7);
            torrent.source = row->getString(8);
            torrent.status = stringToDownloadStatus(row->getString(9));
            torrent.progress = row->getFloat32(10);
            torrent.file_path = row->getString(11);
            torrent.created_at = row->getInt64(12);
            torrent.updated_at = row->getInt64(13);
            return torrent;
        }
        return std::nullopt;
    }

    std::vector<TorrentInfo> Database::getTorrentsForMedia(int media_id) {
        std::vector<TorrentInfo> torrents;
        auto result = m_impl->executor->execute(
            "SELECT * FROM torrents WHERE media_id = ? ORDER BY seeders DESC",
            {std::to_string(media_id)}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                TorrentInfo torrent;
                torrent.id = row->getInt32(0);
                torrent.media_id = row->getInt32(1);
                torrent.info_hash = row->getString(2);
                torrent.magnet_uri = row->getString(3);
                torrent.quality = row->getString(4);
                torrent.size_bytes = row->getInt64(5);
                torrent.seeders = row->getInt32(6);
                torrent.leechers = row->getInt32(7);
                torrent.source = row->getString(8);
                torrent.status = stringToDownloadStatus(row->getString(9));
                torrent.progress = row->getFloat32(10);
                torrent.file_path = row->getString(11);
                torrent.created_at = row->getInt64(12);
                torrent.updated_at = row->getInt64(13);
                torrents.push_back(torrent);
            }
        }
        return torrents;
    }

    std::vector<TorrentInfo> Database::getActiveTorrents() {
        std::vector<TorrentInfo> torrents;
        auto result = m_impl->executor->execute(
            "SELECT * FROM torrents WHERE status IN ('DOWNLOADING', 'SEEDING') ORDER BY updated_at DESC",
            {}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                TorrentInfo torrent;
                torrent.id = row->getInt32(0);
                torrent.media_id = row->getInt32(1);
                torrent.info_hash = row->getString(2);
                torrent.magnet_uri = row->getString(3);
                torrent.quality = row->getString(4);
                torrent.size_bytes = row->getInt64(5);
                torrent.seeders = row->getInt32(6);
                torrent.leechers = row->getInt32(7);
                torrent.source = row->getString(8);
                torrent.status = stringToDownloadStatus(row->getString(9));
                torrent.progress = row->getFloat32(10);
                torrent.file_path = row->getString(11);
                torrent.created_at = row->getInt64(12);
                torrent.updated_at = row->getInt64(13);
                torrents.push_back(torrent);
            }
        }
        return torrents;
    }

    void Database::updateTorrentStatus(int id, DownloadStatus status, float progress) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "UPDATE torrents SET status = ?, progress = ?, updated_at = ? WHERE id = ?",
            {
                downloadStatusToString(status),
                std::to_string(progress),
                std::to_string(now),
                std::to_string(id)
            }
        );
    }

    void Database::updateTorrentFilePath(int id, const std::string& file_path) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "UPDATE torrents SET file_path = ?, updated_at = ? WHERE id = ?",
            {file_path, std::to_string(now), std::to_string(id)}
        );
    }

    void Database::deleteTorrent(int id) {
        m_impl->executor->execute("DELETE FROM torrents WHERE id = ?", {std::to_string(id)});
    }

    // Watch History
    void Database::upsertWatchHistory(const WatchHistory& history) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "INSERT INTO watch_history (media_id, position_seconds, duration_seconds, progress_percent, "
            "last_watched, completed) VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(media_id) DO UPDATE SET "
            "position_seconds = excluded.position_seconds, "
            "duration_seconds = excluded.duration_seconds, "
            "progress_percent = excluded.progress_percent, "
            "last_watched = excluded.last_watched, "
            "completed = excluded.completed",
            {
                std::to_string(history.media_id),
                std::to_string(history.position_seconds),
                std::to_string(history.duration_seconds),
                std::to_string(history.progress_percent),
                std::to_string(now),
                history.completed ? "1" : "0"
            }
        );
    }

    std::optional<WatchHistory> Database::getWatchHistory(int media_id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM watch_history WHERE media_id = ?",
            {std::to_string(media_id)}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            WatchHistory history;
            history.id = row->getInt32(0);
            history.media_id = row->getInt32(1);
            history.position_seconds = row->getInt64(2);
            history.duration_seconds = row->getInt64(3);
            history.progress_percent = row->getFloat32(4);
            history.last_watched = row->getInt64(5);
            history.completed = row->getInt32(6) != 0;
            return history;
        }
        return std::nullopt;
    }

    std::vector<WatchHistory> Database::getRecentlyWatched(int limit) {
        std::vector<WatchHistory> histories;
        auto result = m_impl->executor->execute(
            "SELECT * FROM watch_history ORDER BY last_watched DESC LIMIT ?",
            {std::to_string(limit)}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                WatchHistory history;
                history.id = row->getInt32(0);
                history.media_id = row->getInt32(1);
                history.position_seconds = row->getInt64(2);
                history.duration_seconds = row->getInt64(3);
                history.progress_percent = row->getFloat32(4);
                history.last_watched = row->getInt64(5);
                history.completed = row->getInt32(6) != 0;
                histories.push_back(history);
            }
        }
        return histories;
    }

    void Database::deleteWatchHistory(int media_id) {
        m_impl->executor->execute("DELETE FROM watch_history WHERE media_id = ?", {std::to_string(media_id)});
    }

    // Episodes
    int Database::insertEpisode(const Episode& episode) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->executor->execute(
            "INSERT INTO episodes (media_id, season_number, episode_number, title, description, "
            "still_url, runtime_minutes, air_date, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            {
                std::to_string(episode.media_id),
                std::to_string(episode.season_number),
                std::to_string(episode.episode_number),
                episode.title,
                episode.description,
                episode.still_url,
                std::to_string(episode.runtime_minutes),
                episode.air_date,
                std::to_string(now)
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert episode");
    }

    std::optional<Episode> Database::getEpisode(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM episodes WHERE id = ?",
            {std::to_string(id)}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            Episode episode;
            episode.id = row->getInt32(0);
            episode.media_id = row->getInt32(1);
            episode.season_number = row->getInt32(2);
            episode.episode_number = row->getInt32(3);
            episode.title = row->getString(4);
            episode.description = row->getString(5);
            episode.still_url = row->getString(6);
            episode.runtime_minutes = row->getInt32(7);
            episode.air_date = row->getString(8);
            episode.created_at = row->getInt64(9);
            return episode;
        }
        return std::nullopt;
    }

    std::vector<Episode> Database::getEpisodesForSeries(int media_id, int season_number) {
        std::vector<Episode> episodes;
        auto result = m_impl->executor->execute(
            "SELECT * FROM episodes WHERE media_id = ? AND season_number = ? ORDER BY episode_number ASC",
            {std::to_string(media_id), std::to_string(season_number)}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                Episode episode;
                episode.id = row->getInt32(0);
                episode.media_id = row->getInt32(1);
                episode.season_number = row->getInt32(2);
                episode.episode_number = row->getInt32(3);
                episode.title = row->getString(4);
                episode.description = row->getString(5);
                episode.still_url = row->getString(6);
                episode.runtime_minutes = row->getInt32(7);
                episode.air_date = row->getString(8);
                episode.created_at = row->getInt64(9);
                episodes.push_back(episode);
            }
        }
        return episodes;
    }

    void Database::updateEpisode(const Episode& episode) {
        m_impl->executor->execute(
            "UPDATE episodes SET title = ?, description = ?, still_url = ?, "
            "runtime_minutes = ?, air_date = ? WHERE id = ?",
            {
                episode.title,
                episode.description,
                episode.still_url,
                std::to_string(episode.runtime_minutes),
                episode.air_date,
                std::to_string(episode.id)
            }
        );
    }

    void Database::deleteEpisode(int id) {
        m_impl->executor->execute("DELETE FROM episodes WHERE id = ?", {std::to_string(id)});
    }

    // Statistics
    int Database::getMediaCount(ContentType type) {
        auto result = m_impl->executor->execute(
            "SELECT COUNT(*) FROM media_items WHERE type = ?",
            {contentTypeToString(type)}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            return row->getInt32(0);
        }
        return 0;
    }

    int64_t Database::getTotalDownloadedSize() {
        auto result = m_impl->executor->execute(
            "SELECT SUM(size_bytes) FROM torrents WHERE status = 'COMPLETED'",
            {}
        );

        if (result->isSuccess() && result->hasMoreToFetch()) {
            auto row = result->fetch();
            return row->getInt64(0);
        }
        return 0;
    }

    std::vector<MediaItem> Database::getPopularMedia(ContentType type, int limit) {
        std::vector<MediaItem> items;
        auto result = m_impl->executor->execute(
            "SELECT * FROM media_items WHERE type = ? ORDER BY rating DESC LIMIT ?",
            {contentTypeToString(type), std::to_string(limit)}
        );

        if (result->isSuccess()) {
            while (result->hasMoreToFetch()) {
                auto row = result->fetch();
                MediaItem item;
                item.id = row->getInt32(0);
                item.type = stringToContentType(row->getString(1));
                item.title = row->getString(2);
                item.original_title = row->getString(3);
                item.year = row->getInt32(4);
                item.description = row->getString(5);
                item.poster_url = row->getString(6);
                item.backdrop_url = row->getString(7);
                item.rating = row->getFloat32(8);
                item.genres = row->getString(9);
                item.runtime_minutes = row->getInt32(10);
                item.tmdb_id = row->getString(11);
                item.imdb_id = row->getString(12);
                item.language = row->getString(13);
                item.created_at = row->getInt64(14);
                item.updated_at = row->getInt64(15);
                items.push_back(item);
            }
        }
        return items;
    }

} // namespace media::database

// Made with Bob
