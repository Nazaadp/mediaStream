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
            auto result = executor->execute(sql, std::unordered_map<oatpp::String, oatpp::Void>{});
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
            "VALUES (:type, :title, :original_title, :year, :description, :poster_url, "
            ":backdrop_url, :rating, :genres, :runtime_minutes, :tmdb_id, :imdb_id, :language, :created_at, :updated_at)",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(item.type))},
                {"title", oatpp::String(item.title)},
                {"original_title", oatpp::String(item.original_title)},
                {"year", oatpp::Int32(item.year)},
                {"description", oatpp::String(item.description)},
                {"poster_url", oatpp::String(item.poster_url)},
                {"backdrop_url", oatpp::String(item.backdrop_url)},
                {"rating", oatpp::Float32(item.rating)},
                {"genres", oatpp::String(item.genres)},
                {"runtime_minutes", oatpp::Int32(item.runtime_minutes)},
                {"tmdb_id", oatpp::String(item.tmdb_id)},
                {"imdb_id", oatpp::String(item.imdb_id)},
                {"language", oatpp::String(item.language)},
                {"created_at", oatpp::Int64(now)},
                {"updated_at", oatpp::Int64(now)}
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert media item");
    }

    std::optional<MediaItem> Database::getMediaItem(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM media_items WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            }
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
            "SELECT * FROM media_items WHERE type = :type ORDER BY created_at DESC",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))}
            }
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
            "SELECT * FROM media_items WHERE type = :type AND (title LIKE :query OR original_title LIKE :query) ORDER BY rating DESC",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))},
                {"query", oatpp::String("%" + query + "%")}
            }
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
            "UPDATE media_items SET title = :title, original_title = :original_title, year = :year, description = :description, "
            "poster_url = :poster_url, backdrop_url = :backdrop_url, rating = :rating, genres = :genres, runtime_minutes = :runtime_minutes, "
            "tmdb_id = :tmdb_id, imdb_id = :imdb_id, language = :language, updated_at = :updated_at WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"title", oatpp::String(item.title)},
                {"original_title", oatpp::String(item.original_title)},
                {"year", oatpp::Int32(item.year)},
                {"description", oatpp::String(item.description)},
                {"poster_url", oatpp::String(item.poster_url)},
                {"backdrop_url", oatpp::String(item.backdrop_url)},
                {"rating", oatpp::Float32(item.rating)},
                {"genres", oatpp::String(item.genres)},
                {"runtime_minutes", oatpp::Int32(item.runtime_minutes)},
                {"tmdb_id", oatpp::String(item.tmdb_id)},
                {"imdb_id", oatpp::String(item.imdb_id)},
                {"language", oatpp::String(item.language)},
                {"updated_at", oatpp::Int64(now)},
                {"id", oatpp::Int32(item.id)}
            }
        );
    }

    void Database::deleteMediaItem(int id) {
        m_impl->executor->execute("DELETE FROM media_items WHERE id = :id", std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}});
    }

    // Torrents
    int Database::insertTorrent(const TorrentInfo& torrent) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->executor->execute(
            "INSERT INTO torrents (media_id, info_hash, magnet_uri, quality, size_bytes, seeders, "
            "leechers, source, status, progress, file_path, created_at, updated_at) "
            "VALUES (:media_id, :info_hash, :magnet_uri, :quality, :size_bytes, :seeders, "
            ":leechers, :source, :status, :progress, :file_path, :created_at, :updated_at)",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(torrent.media_id)},
                {"info_hash", oatpp::String(torrent.info_hash)},
                {"magnet_uri", oatpp::String(torrent.magnet_uri)},
                {"quality", oatpp::String(torrent.quality)},
                {"size_bytes", oatpp::Int64(torrent.size_bytes)},
                {"seeders", oatpp::Int32(torrent.seeders)},
                {"leechers", oatpp::Int32(torrent.leechers)},
                {"source", oatpp::String(torrent.source)},
                {"status", oatpp::String(downloadStatusToString(torrent.status))},
                {"progress", oatpp::Float32(torrent.progress)},
                {"file_path", oatpp::String(torrent.file_path)},
                {"created_at", oatpp::Int64(now)},
                {"updated_at", oatpp::Int64(now)}
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert torrent");
    }

    std::optional<TorrentInfo> Database::getTorrent(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM torrents WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            }
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
            "SELECT * FROM torrents WHERE info_hash = :info_hash",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"info_hash", oatpp::String(info_hash)}
            }
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
            "SELECT * FROM torrents WHERE media_id = :media_id ORDER BY seeders DESC",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)}
            }
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
            std::unordered_map<oatpp::String, oatpp::Void>{}
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
            "UPDATE torrents SET status = :status, progress = :progress, updated_at = :updated_at WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"status", oatpp::String(downloadStatusToString(status))},
                {"progress", oatpp::Float32(progress)},
                {"updated_at", oatpp::Int64(now)},
                {"id", oatpp::Int32(id)}
            }
        );
    }

    void Database::updateTorrentFilePath(int id, const std::string& file_path) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "UPDATE torrents SET file_path = :file_path, updated_at = :updated_at WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"file_path", oatpp::String(file_path)},
                {"updated_at", oatpp::Int64(now)},
                {"id", oatpp::Int32(id)}
            }
        );
    }

    void Database::deleteTorrent(int id) {
        m_impl->executor->execute("DELETE FROM torrents WHERE id = :id", std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}});
    }

    // Watch History
    void Database::upsertWatchHistory(const WatchHistory& history) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->executor->execute(
            "INSERT INTO watch_history (media_id, position_seconds, duration_seconds, progress_percent, "
            "last_watched, completed) VALUES (:media_id, :position_seconds, :duration_seconds, :progress_percent, :last_watched, :completed) "
            "ON CONFLICT(media_id) DO UPDATE SET "
            "position_seconds = excluded.position_seconds, "
            "duration_seconds = excluded.duration_seconds, "
            "progress_percent = excluded.progress_percent, "
            "last_watched = excluded.last_watched, "
            "completed = excluded.completed",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(history.media_id)},
                {"position_seconds", oatpp::Int64(history.position_seconds)},
                {"duration_seconds", oatpp::Int64(history.duration_seconds)},
                {"progress_percent", oatpp::Float32(history.progress_percent)},
                {"last_watched", oatpp::Int64(now)},
                {"completed", oatpp::Int32(history.completed ? 1 : 0)}
            }
        );
    }

    std::optional<WatchHistory> Database::getWatchHistory(int media_id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM watch_history WHERE media_id = :media_id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)}
            }
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
            "SELECT * FROM watch_history ORDER BY last_watched DESC LIMIT :limit",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"limit", oatpp::Int32(limit)}
            }
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
        m_impl->executor->execute("DELETE FROM watch_history WHERE media_id = :media_id", std::unordered_map<oatpp::String, oatpp::Void>{{"media_id", oatpp::Int32(media_id)}});
    }

    // Episodes
    int Database::insertEpisode(const Episode& episode) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->executor->execute(
            "INSERT INTO episodes (media_id, season_number, episode_number, title, description, "
            "still_url, runtime_minutes, air_date, created_at) "
            "VALUES (:media_id, :season_number, :episode_number, :title, :description, "
            ":still_url, :runtime_minutes, :air_date, :created_at)",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(episode.media_id)},
                {"season_number", oatpp::Int32(episode.season_number)},
                {"episode_number", oatpp::Int32(episode.episode_number)},
                {"title", oatpp::String(episode.title)},
                {"description", oatpp::String(episode.description)},
                {"still_url", oatpp::String(episode.still_url)},
                {"runtime_minutes", oatpp::Int32(episode.runtime_minutes)},
                {"air_date", oatpp::String(episode.air_date)},
                {"created_at", oatpp::Int64(now)}
            }
        );

        if (result->isSuccess()) {
            return static_cast<int>(m_impl->executor->getConnection()->getLastInsertRowId());
        }
        throw std::runtime_error("Failed to insert episode");
    }

    std::optional<Episode> Database::getEpisode(int id) {
        auto result = m_impl->executor->execute(
            "SELECT * FROM episodes WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            }
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
            "SELECT * FROM episodes WHERE media_id = :media_id AND season_number = :season_number ORDER BY episode_number ASC",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)},
                {"season_number", oatpp::Int32(season_number)}
            }
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
            "UPDATE episodes SET title = :title, description = :description, still_url = :still_url, "
            "runtime_minutes = :runtime_minutes, air_date = :air_date WHERE id = :id",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"title", oatpp::String(episode.title)},
                {"description", oatpp::String(episode.description)},
                {"still_url", oatpp::String(episode.still_url)},
                {"runtime_minutes", oatpp::Int32(episode.runtime_minutes)},
                {"air_date", oatpp::String(episode.air_date)},
                {"id", oatpp::Int32(episode.id)}
            }
        );
    }

    void Database::deleteEpisode(int id) {
        m_impl->executor->execute("DELETE FROM episodes WHERE id = :id", std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}});
    }

    // Statistics
    int Database::getMediaCount(ContentType type) {
        auto result = m_impl->executor->execute(
            "SELECT COUNT(*) FROM media_items WHERE type = :type",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))}
            }
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
            std::unordered_map<oatpp::String, oatpp::Void>{}
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
            "SELECT * FROM media_items WHERE type = :type ORDER BY rating DESC LIMIT :limit",
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))},
                {"limit", oatpp::Int32(limit)}
            }
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
