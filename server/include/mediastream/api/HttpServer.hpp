#pragma once

#include <memory>
#include <thread>
#include <atomic>

// Forward Declaration (We don't need the full header here, saves compile time)
namespace media::core { class TorrentEngine; }
namespace media::services { class ContentDiscoveryManager; }

namespace media::api {

    class HttpServer {
    public:
        // Dependency Injection: The Server needs the Engine to do work
        explicit HttpServer(std::shared_ptr<media::core::TorrentEngine> engine,
                            std::shared_ptr<media::services::ContentDiscoveryManager> discovery);
        ~HttpServer();

        // Lifecycle Management
        void start();
        void stop();

    private:
        void run_internal(); // The actual loop

        std::shared_ptr<media::core::TorrentEngine> m_engine;
        std::shared_ptr<media::services::ContentDiscoveryManager> m_discovery;
        std::thread m_server_thread;
        std::atomic<bool> m_should_run;
    };

} // namespace media::api