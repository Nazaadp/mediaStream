#include "mediastream/database/Database.hpp"
#include <oatpp-sqlite/orm.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace media::database {

#include OATPP_CODEGEN_BEGIN(DbClient)

#include OATPP_CODEGEN_BEGIN(DTO)

class MediaItemDto : public oatpp::DTO {
    DTO_INIT(MediaItemDto, DTO)
    DTO_FIELD(Int32, id);
    DTO_FIELD(String, type);
    DTO_FIELD(String, title);
    DTO_FIELD(String, original_title);
    DTO_FIELD(Int32, year);
    DTO_FIELD(String, description);
    DTO_FIELD(String, poster_url);
    DTO_FIELD(String, backdrop_url);
    DTO_FIELD(Float32, rating);
    DTO_FIELD(String, genres);
    DTO_FIELD(Int32, runtime_minutes);
    DTO_FIELD(String, tmdb_id);
    DTO_FIELD(String, imdb_id);
    DTO_FIELD(String, language);
    DTO_FIELD(String, original_language);
    DTO_FIELD(Int64, created_at);
    DTO_FIELD(Int64, updated_at);
};

class TorrentInfoDto : public oatpp::DTO {
    DTO_INIT(TorrentInfoDto, DTO)
    DTO_FIELD(Int32, id);
    DTO_FIELD(Int32, media_id);
    DTO_FIELD(String, info_hash);
    DTO_FIELD(String, magnet_uri);
    DTO_FIELD(String, quality);
    DTO_FIELD(String, title);
    DTO_FIELD(Int32, season);
    DTO_FIELD(Int32, episode);
    DTO_FIELD(Int32, file_index);
    DTO_FIELD(Int32, is_pack);
    DTO_FIELD(String, type);
    DTO_FIELD(Int64, size_bytes);
    DTO_FIELD(Int32, seeders);
    DTO_FIELD(Int32, leechers);
    DTO_FIELD(String, source);
    DTO_FIELD(String, status);
    DTO_FIELD(Float32, progress);
    DTO_FIELD(String, file_path);
    DTO_FIELD(Int64, created_at);
    DTO_FIELD(Int64, updated_at);
};

class WatchHistoryDto : public oatpp::DTO {
    DTO_INIT(WatchHistoryDto, DTO)
    DTO_FIELD(Int32, id);
    DTO_FIELD(Int32, media_id);
    DTO_FIELD(Int64, position_seconds);
    DTO_FIELD(Int64, duration_seconds);
    DTO_FIELD(Float32, progress_percent);
    DTO_FIELD(Int64, last_watched);
    DTO_FIELD(Int32, completed);
    DTO_FIELD(Int32, last_season);
    DTO_FIELD(Int32, last_episode);
};

class ViewLaterDto : public oatpp::DTO {
    DTO_INIT(ViewLaterDto, DTO)
    DTO_FIELD(Int32, id);
    DTO_FIELD(Int32, media_id);
    DTO_FIELD(Int64, created_at);
};

class EpisodeDto : public oatpp::DTO {
    DTO_INIT(EpisodeDto, DTO)
    DTO_FIELD(Int32, id);
    DTO_FIELD(Int32, media_id);
    DTO_FIELD(Int32, season_number);
    DTO_FIELD(Int32, episode_number);
    DTO_FIELD(String, title);
    DTO_FIELD(String, description);
    DTO_FIELD(String, still_url);
    DTO_FIELD(Int32, runtime_minutes);
    DTO_FIELD(String, air_date);
    DTO_FIELD(Int64, created_at);
};

class IntResultDto : public oatpp::DTO {
    DTO_INIT(IntResultDto, DTO)
    DTO_FIELD(Int32, value);
};

class Int64ResultDto : public oatpp::DTO {
    DTO_INIT(Int64ResultDto, DTO)
    DTO_FIELD(Int64, value);
};

class TextResultDto : public oatpp::DTO {
    DTO_INIT(TextResultDto, DTO)
    DTO_FIELD(String, value);
};

#include OATPP_CODEGEN_END(DTO)

class AppDbClient : public oatpp::orm::DbClient {
public:
    AppDbClient(const std::shared_ptr<oatpp::sqlite::Executor>& executor)
        : oatpp::orm::DbClient(executor) {}
};
#include OATPP_CODEGEN_END(DbClient)


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

    namespace {
        MediaItem rowToMediaItem(const oatpp::Object<MediaItemDto>& row) {
            MediaItem item;
            item.id = row->id ? *row->id : 0;
            item.type = stringToContentType(row->type ? *row->type : "MOVIE");
            item.title = row->title ? *row->title : "";
            item.original_title = row->original_title ? *row->original_title : "";
            item.year = row->year ? *row->year : 0;
            item.description = row->description ? *row->description : "";
            item.poster_url = row->poster_url ? *row->poster_url : "";
            item.backdrop_url = row->backdrop_url ? *row->backdrop_url : "";
            item.rating = row->rating ? *row->rating : 0.0f;
            item.genres = row->genres ? *row->genres : "";
            item.runtime_minutes = row->runtime_minutes ? *row->runtime_minutes : 0;
            item.tmdb_id = row->tmdb_id ? *row->tmdb_id : "";
            item.imdb_id = row->imdb_id ? *row->imdb_id : "";
            item.language = row->language ? *row->language : "";
            item.original_language = row->original_language ? *row->original_language : "";
            item.created_at = row->created_at ? *row->created_at : 0;
            item.updated_at = row->updated_at ? *row->updated_at : 0;
            return item;
        }

        TorrentInfo rowToTorrentInfo(const oatpp::Object<TorrentInfoDto>& row) {
            TorrentInfo info;
            info.id = row->id ? *row->id : 0;
            info.media_id = row->media_id ? *row->media_id : 0;
            info.info_hash = row->info_hash ? *row->info_hash : "";
            info.magnet_uri = row->magnet_uri ? *row->magnet_uri : "";
            info.quality = row->quality ? *row->quality : "";
            info.title = row->title ? *row->title : "";
            info.season = row->season ? *row->season : 0;
            info.episode = row->episode ? *row->episode : 0;
            info.file_index = row->file_index ? *row->file_index : -1;
            info.is_pack = row->is_pack ? (*row->is_pack != 0) : false;
            info.type = row->type ? *row->type : "";
            info.size_bytes = row->size_bytes ? *row->size_bytes : 0;
            info.seeders = row->seeders ? *row->seeders : 0;
            info.leechers = row->leechers ? *row->leechers : 0;
            info.source = row->source ? *row->source : "";
            info.status = stringToDownloadStatus(row->status ? *row->status : "PENDING");
            info.progress = row->progress ? *row->progress : 0.0f;
            info.file_path = row->file_path ? *row->file_path : "";
            info.created_at = row->created_at ? *row->created_at : 0;
            info.updated_at = row->updated_at ? *row->updated_at : 0;
            return info;
        }

        Episode rowToEpisode(const oatpp::Object<EpisodeDto>& row) {
            Episode ep;
            ep.id = row->id ? *row->id : 0;
            ep.media_id = row->media_id ? *row->media_id : 0;
            ep.season_number = row->season_number ? *row->season_number : 0;
            ep.episode_number = row->episode_number ? *row->episode_number : 0;
            ep.title = row->title ? *row->title : "";
            ep.description = row->description ? *row->description : "";
            ep.still_url = row->still_url ? *row->still_url : "";
            ep.runtime_minutes = row->runtime_minutes ? *row->runtime_minutes : 0;
            ep.air_date = row->air_date ? *row->air_date : "";
            ep.created_at = row->created_at ? *row->created_at : 0;
            return ep;
        }
    }

    class Database::Impl {
    public:
        std::shared_ptr<oatpp::sqlite::Executor> executor;
        std::shared_ptr<AppDbClient> client;
        
        explicit Impl(const std::filesystem::path& db_path) {
            auto connectionProvider = std::make_shared<oatpp::sqlite::ConnectionProvider>(db_path.string());
            executor = std::make_shared<oatpp::sqlite::Executor>(connectionProvider);
            client = std::make_shared<AppDbClient>(executor);
            spdlog::info("Database initialized at: {}", db_path.string());
        }

        void executeSQL(const std::string& sql) {
            auto result = client->executeQuery(oatpp::String(sql), std::unordered_map<oatpp::String, oatpp::Void>{});
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

        m_impl->executeSQL("PRAGMA journal_mode=WAL");
        m_impl->executeSQL("PRAGMA synchronous=NORMAL");
        m_impl->executeSQL("PRAGMA foreign_keys=ON");


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
                original_language TEXT DEFAULT '',
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            )
        )");

        // Torrents Table.
        // Uniqueness is (info_hash, file_index), NOT info_hash alone: season
        // packs register one row per episode file under the same infohash —
        // a hash-only constraint made episode 2's row overwrite episode 1's.
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS torrents (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                info_hash TEXT NOT NULL,
                magnet_uri TEXT NOT NULL,
                quality TEXT,
                title TEXT,
                season INTEGER DEFAULT 0,
                episode INTEGER DEFAULT 0,
                file_index INTEGER DEFAULT -1,
                is_pack INTEGER DEFAULT 0,
                type TEXT,
                size_bytes INTEGER DEFAULT 0,
                seeders INTEGER DEFAULT 0,
                leechers INTEGER DEFAULT 0,
                source TEXT,
                status TEXT DEFAULT 'PENDING',
                progress REAL DEFAULT 0.0,
                file_path TEXT,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL,
                FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE,
                UNIQUE(info_hash, file_index)
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
                last_season INTEGER,
                last_episode INTEGER,
                FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE,
                UNIQUE(media_id)
            )
        )");

        // View Later Table
        m_impl->executeSQL(R"(
            CREATE TABLE IF NOT EXISTS view_later (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                media_id INTEGER NOT NULL,
                created_at INTEGER NOT NULL,
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
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_media_tmdb ON media_items(tmdb_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_hash ON torrents(info_hash)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_media ON torrents(media_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_torrents_status ON torrents(status)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_watch_history_media ON watch_history(media_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_view_later_media ON view_later(media_id)");
        m_impl->executeSQL("CREATE INDEX IF NOT EXISTS idx_episodes_media ON episodes(media_id)");

        // Add original_language to media_items if missing (migration for existing DBs)
        try {
            m_impl->executeSQL("ALTER TABLE media_items ADD COLUMN original_language TEXT DEFAULT ''");
        } catch (...) {}

        // Try to add the 'type' column to torrents table if it already exists (migration)
        try {
            m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN type TEXT");
        } catch (...) {
            // Safe to ignore, column already exists
        }

        // Add last_season / last_episode to watch_history (migration for existing DBs)
        try { m_impl->executeSQL("ALTER TABLE watch_history ADD COLUMN last_season INTEGER"); } catch (...) {}
        try { m_impl->executeSQL("ALTER TABLE watch_history ADD COLUMN last_episode INTEGER"); } catch (...) {}

        // Add title / season / episode to torrents (migration for existing DBs).
        // title = full release name; season/episode let the client resolve
        // "which episode is this torrent" without regex-matching names.
        try { m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN title TEXT"); } catch (...) {}
        try { m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN season INTEGER DEFAULT 0"); } catch (...) {}
        try { m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN episode INTEGER DEFAULT 0"); } catch (...) {}

        // Add file_index / is_pack (season-pack support, migration for existing DBs)
        try { m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN file_index INTEGER DEFAULT -1"); } catch (...) {}
        try { m_impl->executeSQL("ALTER TABLE torrents ADD COLUMN is_pack INTEGER DEFAULT 0"); } catch (...) {}

        migrateTorrentsUniqueConstraint();

        spdlog::info("Database schema created successfully");
    }

    // Legacy DBs declared `info_hash TEXT UNIQUE`, which collapses every
    // episode of a season pack into one row (same infohash, different file).
    // SQLite cannot drop a constraint in place, so when the old inline UNIQUE
    // is still present the table is rebuilt with UNIQUE(info_hash, file_index).
    // Runs on one pinned connection so the transaction is real (the executor
    // pools connections; BEGIN/COMMIT on separate executeQuery calls would
    // land on different handles).
    void Database::migrateTorrentsUniqueConstraint() {
        try {
            auto result = m_impl->client->executeQuery(oatpp::String(
                "SELECT sql AS value FROM sqlite_master WHERE type='table' AND name='torrents'"),
                std::unordered_map<oatpp::String, oatpp::Void>{});
            if (!result->isSuccess()) return;
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<TextResultDto>>>();
            if (!dataset || dataset->empty() || !dataset->front()->value) return;

            const std::string ddl = *dataset->front()->value;
            if (ddl.find("info_hash TEXT UNIQUE") == std::string::npos) {
                return; // Already on the composite constraint
            }
            spdlog::info("Migrating torrents table to UNIQUE(info_hash, file_index)...");

            auto connection = m_impl->executor->getConnection();
            auto exec = [&](const char* sql) {
                auto r = m_impl->client->executeQuery(oatpp::String(sql),
                    std::unordered_map<oatpp::String, oatpp::Void>{}, connection);
                if (!r->isSuccess()) {
                    throw std::runtime_error(std::string("migration step failed: ") + sql);
                }
            };

            exec("BEGIN");
            try {
                exec(R"(CREATE TABLE torrents_migr (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        media_id INTEGER NOT NULL,
                        info_hash TEXT NOT NULL,
                        magnet_uri TEXT NOT NULL,
                        quality TEXT,
                        title TEXT,
                        season INTEGER DEFAULT 0,
                        episode INTEGER DEFAULT 0,
                        file_index INTEGER DEFAULT -1,
                        is_pack INTEGER DEFAULT 0,
                        type TEXT,
                        size_bytes INTEGER DEFAULT 0,
                        seeders INTEGER DEFAULT 0,
                        leechers INTEGER DEFAULT 0,
                        source TEXT,
                        status TEXT DEFAULT 'PENDING',
                        progress REAL DEFAULT 0.0,
                        file_path TEXT,
                        created_at INTEGER NOT NULL,
                        updated_at INTEGER NOT NULL,
                        FOREIGN KEY (media_id) REFERENCES media_items(id) ON DELETE CASCADE,
                        UNIQUE(info_hash, file_index)
                    ))");
                exec("INSERT INTO torrents_migr (id, media_id, info_hash, magnet_uri, quality, title, "
                     "season, episode, file_index, is_pack, type, size_bytes, seeders, leechers, "
                     "source, status, progress, file_path, created_at, updated_at) "
                     "SELECT id, media_id, info_hash, magnet_uri, quality, title, "
                     "season, episode, COALESCE(file_index, -1), COALESCE(is_pack, 0), type, size_bytes, seeders, leechers, "
                     "source, status, progress, file_path, created_at, updated_at FROM torrents");
                exec("DROP TABLE torrents");
                exec("ALTER TABLE torrents_migr RENAME TO torrents");
                exec("CREATE INDEX IF NOT EXISTS idx_torrents_hash ON torrents(info_hash)");
                exec("CREATE INDEX IF NOT EXISTS idx_torrents_media ON torrents(media_id)");
                exec("CREATE INDEX IF NOT EXISTS idx_torrents_status ON torrents(status)");
                exec("COMMIT");
                spdlog::info("Torrents table migration completed");
            } catch (...) {
                try {
                    m_impl->client->executeQuery(oatpp::String("ROLLBACK"),
                        std::unordered_map<oatpp::String, oatpp::Void>{}, connection);
                } catch (...) {}
                throw;
            }
        } catch (const std::exception& e) {
            spdlog::error("Torrents unique-constraint migration failed: {}", e.what());
        }
    }

    void Database::migrate() {
        // Future migrations will go here
        spdlog::info("Database migrations completed");
    }

    // Media Items
    int Database::insertMediaItem(const MediaItem& item) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->client->executeQuery(oatpp::String("INSERT INTO media_items (type, title, original_title, year, description, poster_url, "
            "backdrop_url, rating, genres, runtime_minutes, tmdb_id, imdb_id, language, original_language, created_at, updated_at) "
            "VALUES (:type, :title, :original_title, :year, :description, :poster_url, "
            ":backdrop_url, :rating, :genres, :runtime_minutes, :tmdb_id, :imdb_id, :language, :original_language, :created_at, :updated_at)"), std::unordered_map<oatpp::String, oatpp::Void>{
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
                {"tmdb_id", item.tmdb_id.empty() ? oatpp::String() : oatpp::String(item.tmdb_id)},
                {"imdb_id", item.imdb_id.empty() ? oatpp::String() : oatpp::String(item.imdb_id)},
                {"language", oatpp::String(item.language)},
                {"original_language", oatpp::String(item.original_language)},
                {"created_at", oatpp::Int64(now)},
                {"updated_at", oatpp::Int64(now)}
            });

        if (result->isSuccess()) {
            auto idResult = m_impl->client->executeQuery(oatpp::String("SELECT last_insert_rowid() AS value"), std::unordered_map<oatpp::String, oatpp::Void>{});
            if (idResult->isSuccess()) {
                auto dataset = idResult->fetch<oatpp::Vector<oatpp::Object<Int64ResultDto>>>();
                if (dataset && dataset->size() > 0 && dataset->front()->value) {
                    int last_id = static_cast<int>(*dataset->front()->value);
                    if (last_id > 0) return last_id;
                }
            }
        }
        
        // If last_insert_rowid() failed or returned 0 (e.g., due to INSERT OR IGNORE over connection pool)
        // explicitly fetch the ID we just inserted/ignored
        if (!item.tmdb_id.empty()) {
            auto existing = getMediaItemByTmdbId(item.tmdb_id);
            if (existing) return existing->id;
        }
        if (!item.imdb_id.empty()) {
            auto existing = getMediaItemByImdbId(item.imdb_id);
            if (existing) return existing->id;
        }
        auto fallbackResult = m_impl->client->executeQuery(oatpp::String("SELECT id AS value FROM media_items WHERE title = :title AND year = :year"), std::unordered_map<oatpp::String, oatpp::Void>{
            {"title", oatpp::String(item.title)},
            {"year", oatpp::Int32(item.year)}
        });
        if (fallbackResult->isSuccess()) {
            auto dataset = fallbackResult->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
            if (dataset && dataset->size() > 0 && dataset->front()->value) {
                return *dataset->front()->value;
            }
        }
        
        throw std::runtime_error("Failed to insert media item");
    }

    int Database::upsertMediaItem(const MediaItem& item) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();

        // Helper lambda: update type + original_language on an existing record so
        // that items previously stored with wrong type get corrected on next save.
        auto fixType = [&](int existing_id) {
            m_impl->client->executeQuery(
                oatpp::String("UPDATE media_items SET type = :type, original_language = :original_language, updated_at = :updated_at WHERE id = :id"),
                std::unordered_map<oatpp::String, oatpp::Void>{
                    {"type",              oatpp::String(contentTypeToString(item.type))},
                    {"original_language", oatpp::String(item.original_language)},
                    {"updated_at",        oatpp::Int64(now)},
                    {"id",                oatpp::Int32(existing_id)},
                });
            return existing_id;
        };

        if (!item.tmdb_id.empty()) {
            auto existing = getMediaItemByTmdbId(item.tmdb_id);
            if (existing) return fixType(existing->id);
        }
        if (!item.imdb_id.empty()) {
            auto existing = getMediaItemByImdbId(item.imdb_id);
            if (existing) return fixType(existing->id);
        }

        // Also fallback to checking Title AND Year as a last resort UNIQUE check
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT id AS value FROM media_items WHERE title = :title AND year = :year"), std::unordered_map<oatpp::String, oatpp::Void>{
            {"title", oatpp::String(item.title)},
            {"year", oatpp::Int32(item.year)}
        });
        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
            if (dataset && dataset->size() > 0 && dataset->front()->value) {
                return fixType(*dataset->front()->value);
            }
        }

        // Not found — insert new record
        return insertMediaItem(item);
    }

    std::optional<MediaItem> Database::getMediaItem(int id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToMediaItem(row);
            }
        }
        return std::nullopt;
    }

    std::optional<MediaItem> Database::getMediaItemByTmdbId(const std::string& tmdb_id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE tmdb_id = :tmdb_id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"tmdb_id", oatpp::String(tmdb_id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToMediaItem(row);
            }
        }
        return std::nullopt;
    }

    std::optional<MediaItem> Database::getMediaItemByImdbId(const std::string& imdb_id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE imdb_id = :imdb_id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"imdb_id", oatpp::String(imdb_id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToMediaItem(row);
            }
        }
        return std::nullopt;
    }

    std::vector<MediaItem> Database::getAllMedia(ContentType type) {
        std::vector<MediaItem> items;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE type = :type ORDER BY created_at DESC"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    items.push_back(rowToMediaItem(row));
                }
            }
        }
        return items;
    }

    std::vector<MediaItem> Database::searchMedia(const std::string& query, ContentType type) {
        std::vector<MediaItem> items;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE type = :type AND (title LIKE :query OR original_title LIKE :query) ORDER BY rating DESC"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))},
                {"query", oatpp::String("%" + query + "%")}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    items.push_back(rowToMediaItem(row));
                }
            }
        }
        return items;
    }

    void Database::updateMediaItem(const MediaItem& item) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->client->executeQuery(oatpp::String("UPDATE media_items SET title = :title, original_title = :original_title, year = :year, description = :description, "
            "poster_url = :poster_url, backdrop_url = :backdrop_url, rating = :rating, genres = :genres, runtime_minutes = :runtime_minutes, "
            "tmdb_id = :tmdb_id, imdb_id = :imdb_id, language = :language, updated_at = :updated_at WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
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
            });
    }

    void Database::deleteMediaItem(int id) {
        auto connection = m_impl->executor->getConnection();
        m_impl->client->executeQuery(oatpp::String("PRAGMA foreign_keys=ON"), std::unordered_map<oatpp::String, oatpp::Void>{}, connection);
        m_impl->client->executeQuery(oatpp::String("DELETE FROM media_items WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}}, connection);
    }

    // Torrents
    int Database::insertTorrent(const TorrentInfo& torrent) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Upsert on (info_hash, file_index): a torrent replayed later (e.g.
        // once title/season/episode are known) refreshes its metadata instead
        // of being dropped by the UNIQUE constraint. Season packs get one row
        // PER FILE — hash-only conflict made episode 2 overwrite episode 1.
        // Engine-managed fields (status, progress, file_path) are left
        // untouched; title/season/episode only overwrite when the incoming
        // value is meaningful, so a metadata-less save never wipes previously
        // stored data.
        auto result = m_impl->client->executeQuery(oatpp::String("INSERT INTO torrents (media_id, info_hash, magnet_uri, quality, title, season, episode, file_index, is_pack, type, size_bytes, seeders, "
            "leechers, source, status, progress, file_path, created_at, updated_at) "
            "VALUES (:media_id, :info_hash, :magnet_uri, :quality, :title, :season, :episode, :file_index, :is_pack, :type, :size_bytes, :seeders, "
            ":leechers, :source, :status, :progress, :file_path, :created_at, :updated_at) "
            "ON CONFLICT(info_hash, file_index) DO UPDATE SET "
            "title = CASE WHEN excluded.title != '' THEN excluded.title ELSE torrents.title END, "
            "season = CASE WHEN excluded.season > 0 THEN excluded.season ELSE torrents.season END, "
            "episode = CASE WHEN excluded.episode > 0 THEN excluded.episode ELSE torrents.episode END, "
            "is_pack = CASE WHEN excluded.is_pack != 0 THEN excluded.is_pack ELSE torrents.is_pack END, "
            "seeders = excluded.seeders, "
            "leechers = excluded.leechers, "
            "updated_at = excluded.updated_at"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(torrent.media_id)},
                {"info_hash", oatpp::String(torrent.info_hash)},
                {"magnet_uri", oatpp::String(torrent.magnet_uri)},
                {"quality", oatpp::String(torrent.quality)},
                {"title", oatpp::String(torrent.title)},
                {"season", oatpp::Int32(torrent.season)},
                {"episode", oatpp::Int32(torrent.episode)},
                {"file_index", oatpp::Int32(torrent.file_index)},
                {"is_pack", oatpp::Int32(torrent.is_pack ? 1 : 0)},
                {"type", oatpp::String(torrent.type)},
                {"size_bytes", oatpp::Int64(torrent.size_bytes)},
                {"seeders", oatpp::Int32(torrent.seeders)},
                {"leechers", oatpp::Int32(torrent.leechers)},
                {"source", oatpp::String(torrent.source)},
                {"status", oatpp::String(downloadStatusToString(torrent.status))},
                {"progress", oatpp::Float32(torrent.progress)},
                {"file_path", oatpp::String(torrent.file_path)},
                {"created_at", oatpp::Int64(now)},
                {"updated_at", oatpp::Int64(now)}
            });

        if (result->isSuccess()) {
            auto idResult = m_impl->client->executeQuery(oatpp::String("SELECT id AS value FROM torrents WHERE info_hash = :info_hash AND file_index = :file_index"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"info_hash", oatpp::String(torrent.info_hash)},
                {"file_index", oatpp::Int32(torrent.file_index)}
            });
            if (idResult->isSuccess()) {
                auto dataset = idResult->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
                if (dataset && dataset->size() > 0) {
                    return dataset->front()->value ? *dataset->front()->value : 0;
                }
            }
        }
        throw std::runtime_error("Failed to insert torrent");
    }

    std::optional<TorrentInfo> Database::getTorrent(int id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM torrents WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<TorrentInfoDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToTorrentInfo(row);
            }
        }
        return std::nullopt;
    }

    std::optional<TorrentInfo> Database::getTorrentByHash(const std::string& info_hash) {
        // NOCASE: discovery sources store hashes in mixed case (YTS uppercase)
        // while libtorrent reports lowercase — the lookup must match both.
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM torrents WHERE info_hash = :info_hash COLLATE NOCASE"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"info_hash", oatpp::String(info_hash)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<TorrentInfoDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToTorrentInfo(row);
            }
        }
        return std::nullopt;
    }

    std::vector<TorrentInfo> Database::getTorrentsForMedia(int media_id) {
        std::vector<TorrentInfo> torrents;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM torrents WHERE media_id = :media_id ORDER BY seeders DESC"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<TorrentInfoDto>>>();
            if (dataset) {
                for(auto& row : *dataset) {
                    torrents.push_back(rowToTorrentInfo(row));
                }
            }
        }
        return torrents;
    }

    std::vector<TorrentInfo> Database::getActiveTorrents() {
        std::vector<TorrentInfo> torrents;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM torrents WHERE status IN ('DOWNLOADING', 'SEEDING') ORDER BY updated_at DESC"), std::unordered_map<oatpp::String, oatpp::Void>{});

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<TorrentInfoDto>>>();
            if (dataset) {
                for(auto& row : *dataset) {
                    torrents.push_back(rowToTorrentInfo(row));
                }
            }
        }
        return torrents;
    }

    void Database::updateTorrentStatus(int id, DownloadStatus status, float progress) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->client->executeQuery(oatpp::String("UPDATE torrents SET status = :status, progress = :progress, updated_at = :updated_at WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"status", oatpp::String(downloadStatusToString(status))},
                {"progress", oatpp::Float32(progress)},
                {"updated_at", oatpp::Int64(now)},
                {"id", oatpp::Int32(id)}
            });
    }

    void Database::updateTorrentFilePath(int id, const std::string& file_path) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        m_impl->client->executeQuery(oatpp::String("UPDATE torrents SET file_path = :file_path, updated_at = :updated_at WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"file_path", oatpp::String(file_path)},
                {"updated_at", oatpp::Int64(now)},
                {"id", oatpp::Int32(id)}
            });
    }

    void Database::deleteTorrent(int id) {
        m_impl->client->executeQuery(oatpp::String("DELETE FROM torrents WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}});
    }

    // Watch History
    void Database::upsertWatchHistory(const WatchHistory& history) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();

        // last_season / last_episode are only updated when > 0 — passing 0 from
        // a movie save must NOT clobber an existing series episode pointer.
        m_impl->client->executeQuery(oatpp::String(
            "INSERT INTO watch_history (media_id, position_seconds, duration_seconds, progress_percent, "
            "last_watched, completed, last_season, last_episode) "
            "VALUES (:media_id, :position_seconds, :duration_seconds, :progress_percent, :last_watched, :completed, :last_season, :last_episode) "
            "ON CONFLICT(media_id) DO UPDATE SET "
            "position_seconds = excluded.position_seconds, "
            "duration_seconds = excluded.duration_seconds, "
            "progress_percent = excluded.progress_percent, "
            "last_watched = excluded.last_watched, "
            "completed = excluded.completed, "
            "last_season = CASE WHEN excluded.last_season > 0 THEN excluded.last_season ELSE watch_history.last_season END, "
            "last_episode = CASE WHEN excluded.last_episode > 0 THEN excluded.last_episode ELSE watch_history.last_episode END"),
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(history.media_id)},
                {"position_seconds", oatpp::Int64(history.position_seconds)},
                {"duration_seconds", oatpp::Int64(history.duration_seconds)},
                {"progress_percent", oatpp::Float32(history.progress_percent)},
                {"last_watched", oatpp::Int64(now)},
                {"completed", oatpp::Int32(history.completed ? 1 : 0)},
                {"last_season", oatpp::Int32(history.last_season)},
                {"last_episode", oatpp::Int32(history.last_episode)}
            });
    }

    std::optional<WatchHistory> Database::getWatchHistory(int media_id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM watch_history WHERE media_id = :media_id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<WatchHistoryDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                WatchHistory history;
                history.id = row->id ? *row->id : 0;
                history.media_id = row->media_id ? *row->media_id : 0;
                history.position_seconds = row->position_seconds ? *row->position_seconds : 0;
                history.duration_seconds = row->duration_seconds ? *row->duration_seconds : 0;
                history.progress_percent = row->progress_percent ? *row->progress_percent : 0.0f;
                history.last_watched = row->last_watched ? *row->last_watched : 0;
                history.completed = (row->completed ? *row->completed : 0) != 0;
                history.last_season = row->last_season ? *row->last_season : 0;
                history.last_episode = row->last_episode ? *row->last_episode : 0;
                return history;
            }
        }
        return std::nullopt;
    }

    std::vector<WatchHistory> Database::getRecentlyWatched(int limit) {
        std::vector<WatchHistory> histories;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM watch_history ORDER BY last_watched DESC LIMIT :limit"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"limit", oatpp::Int32(limit)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<WatchHistoryDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    WatchHistory history;
                    history.id = row->id ? *row->id : 0;
                    history.media_id = row->media_id ? *row->media_id : 0;
                    history.position_seconds = row->position_seconds ? *row->position_seconds : 0;
                    history.duration_seconds = row->duration_seconds ? *row->duration_seconds : 0;
                    history.progress_percent = row->progress_percent ? *row->progress_percent : 0.0f;
                    history.last_watched = row->last_watched ? *row->last_watched : 0;
                    history.completed = (row->completed ? *row->completed : 0) != 0;
                history.last_season = row->last_season ? *row->last_season : 0;
                history.last_episode = row->last_episode ? *row->last_episode : 0;
                    histories.push_back(history);
                }
            }
        }
        return histories;
    }

    std::vector<MediaItem> Database::getWatchHistoryMedia(int limit) {
        std::vector<MediaItem> items;
        auto result = m_impl->client->executeQuery(oatpp::String(
            "SELECT m.* FROM media_items m "
            "JOIN watch_history w ON m.id = w.media_id "
            "ORDER BY w.last_watched DESC LIMIT :limit"), 
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"limit", oatpp::Int32(limit)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    items.push_back(rowToMediaItem(row));
                }
            }
        }
        return items;
    }

    void Database::deleteWatchHistory(int media_id) {
        m_impl->client->executeQuery(oatpp::String("DELETE FROM watch_history WHERE media_id = :media_id"), std::unordered_map<oatpp::String, oatpp::Void>{{"media_id", oatpp::Int32(media_id)}});
    }

    // View Later
    void Database::toggleViewLater(int media_id, bool saved) {
        if (saved) {
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            m_impl->client->executeQuery(oatpp::String("INSERT OR IGNORE INTO view_later (media_id, created_at) VALUES (:media_id, :created_at)"), 
                std::unordered_map<oatpp::String, oatpp::Void>{
                    {"media_id", oatpp::Int32(media_id)},
                    {"created_at", oatpp::Int64(now)}
                });
        } else {
            m_impl->client->executeQuery(oatpp::String("DELETE FROM view_later WHERE media_id = :media_id"), 
                std::unordered_map<oatpp::String, oatpp::Void>{{"media_id", oatpp::Int32(media_id)}});
        }
    }

    bool Database::isViewLater(int media_id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT COUNT(*) AS value FROM view_later WHERE media_id = :media_id"), 
            std::unordered_map<oatpp::String, oatpp::Void>{{"media_id", oatpp::Int32(media_id)}});
        
        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
            if (dataset && dataset->size() > 0) return (dataset->front()->value ? *dataset->front()->value : 0) > 0;
        }
        return false;
    }

    std::vector<MediaItem> Database::getViewLaterMedia(int limit) {
        std::vector<MediaItem> items;
        auto result = m_impl->client->executeQuery(oatpp::String(
            "SELECT m.* FROM media_items m "
            "JOIN view_later v ON m.id = v.media_id "
            "ORDER BY v.created_at DESC LIMIT :limit"), 
            std::unordered_map<oatpp::String, oatpp::Void>{
                {"limit", oatpp::Int32(limit)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    items.push_back(rowToMediaItem(row));
                }
            }
        }
        return items;
    }

    // Episodes
    int Database::insertEpisode(const Episode& episode) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto result = m_impl->client->executeQuery(oatpp::String("INSERT INTO episodes (media_id, season_number, episode_number, title, description, "
            "still_url, runtime_minutes, air_date, created_at) "
            "VALUES (:media_id, :season_number, :episode_number, :title, :description, "
            ":still_url, :runtime_minutes, :air_date, :created_at)"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(episode.media_id)},
                {"season_number", oatpp::Int32(episode.season_number)},
                {"episode_number", oatpp::Int32(episode.episode_number)},
                {"title", oatpp::String(episode.title)},
                {"description", oatpp::String(episode.description)},
                {"still_url", oatpp::String(episode.still_url)},
                {"runtime_minutes", oatpp::Int32(episode.runtime_minutes)},
                {"air_date", oatpp::String(episode.air_date)},
                {"created_at", oatpp::Int64(now)}
            });

        if (result->isSuccess()) {
            auto idResult = m_impl->client->executeQuery(oatpp::String("SELECT id AS value FROM episodes WHERE media_id = :media_id AND season_number = :season_number AND episode_number = :episode_number"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(episode.media_id)},
                {"season_number", oatpp::Int32(episode.season_number)},
                {"episode_number", oatpp::Int32(episode.episode_number)}
            });
            if (idResult->isSuccess()) {
                auto dataset = idResult->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
                if (dataset && dataset->size() > 0) {
                    return dataset->front()->value ? *dataset->front()->value : 0;
                }
            }
        }
        throw std::runtime_error("Failed to insert episode");
    }

    std::optional<Episode> Database::getEpisode(int id) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM episodes WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"id", oatpp::Int32(id)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<EpisodeDto>>>();
            if (dataset && dataset->size() > 0) {
                auto row = dataset->front();
                return rowToEpisode(row);
            }
        }
        return std::nullopt;
    }

    std::vector<Episode> Database::getEpisodesForSeries(int media_id, int season_number) {
        std::vector<Episode> episodes;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM episodes WHERE media_id = :media_id AND season_number = :season_number ORDER BY episode_number ASC"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"media_id", oatpp::Int32(media_id)},
                {"season_number", oatpp::Int32(season_number)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<EpisodeDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    episodes.push_back(rowToEpisode(row));
                }
            }
        }
        return episodes;
    }

    void Database::updateEpisode(const Episode& episode) {
        m_impl->client->executeQuery(oatpp::String("UPDATE episodes SET title = :title, description = :description, still_url = :still_url, "
            "runtime_minutes = :runtime_minutes, air_date = :air_date WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"title", oatpp::String(episode.title)},
                {"description", oatpp::String(episode.description)},
                {"still_url", oatpp::String(episode.still_url)},
                {"runtime_minutes", oatpp::Int32(episode.runtime_minutes)},
                {"air_date", oatpp::String(episode.air_date)},
                {"id", oatpp::Int32(episode.id)}
            });
    }

    void Database::deleteEpisode(int id) {
        m_impl->client->executeQuery(oatpp::String("DELETE FROM episodes WHERE id = :id"), std::unordered_map<oatpp::String, oatpp::Void>{{"id", oatpp::Int32(id)}});
    }

    // Statistics
    int Database::getMediaCount(ContentType type) {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT COUNT(*) AS value FROM media_items WHERE type = :type"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<IntResultDto>>>();
            if (dataset && dataset->size() > 0) return dataset->front()->value ? *dataset->front()->value : 0;
        }
        return 0;
    }

    int64_t Database::getTotalDownloadedSize() {
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT SUM(size_bytes) AS value FROM torrents WHERE status = 'COMPLETED'"), std::unordered_map<oatpp::String, oatpp::Void>{});

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<Int64ResultDto>>>();
            if (dataset && dataset->size() > 0) return dataset->front()->value ? *dataset->front()->value : 0;
        }
        return 0;
    }

    std::vector<MediaItem> Database::getPopularMedia(ContentType type, int limit) {
        std::vector<MediaItem> items;
        auto result = m_impl->client->executeQuery(oatpp::String("SELECT * FROM media_items WHERE type = :type ORDER BY rating DESC LIMIT :limit"), std::unordered_map<oatpp::String, oatpp::Void>{
                {"type", oatpp::String(contentTypeToString(type))},
                {"limit", oatpp::Int32(limit)}
            });

        if (result->isSuccess()) {
            auto dataset = result->fetch<oatpp::Vector<oatpp::Object<MediaItemDto>>>();
            if (dataset) {
                for (auto& row : *dataset) {
                    items.push_back(rowToMediaItem(row));
                }
            }
        }
        return items;
    }

} // namespace media::database

// Made with Bob
