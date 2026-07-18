#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <filesystem>

// Third-party
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "oatpp/core/base/Environment.hpp"

// Internal Domain
#include "mediastream/core/TorrentEngine.hpp"
#include "mediastream/services/ContentDiscovery.hpp"
#include "mediastream/api/HttpServer.hpp"
#include "mediastream/database/Database.hpp"

// Platform specific (Linux) for User ID checks
#if defined(__linux__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

// --- GLOBAL SIGNAL STATE ---
// Standard C++ approach to handle OS signals (SIGINT, SIGTERM)
// atomic<bool> ensures thread safety without locks.
std::atomic<bool> g_keep_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::warn("Shutdown signal received (Signal: {}). Stopping services...", signal);
        g_keep_running = false;
    }
}

// --- SECURITY CHECKS ---
void enforce_security_context() {
#if defined(__linux__)
    // 1. Root Check
    // A complex parser/network service should NEVER run as root.
    // If the attacker finds a buffer overflow in libtorrent, they get root access.
    if (geteuid() == 0) {
        spdlog::critical("SECURITY VIOLATION: Refusing to run as Root/Sudo.");
        spdlog::critical("Create a dedicated user (e.g., 'mediastream') and run as that user.");
        std::exit(EXIT_FAILURE);
    }

    // 2. Umask (File Permissions)
    // Set default file creation permissions to 700 (Owner only).
    // This prevents other users on the server from reading downloaded media or logs.
    umask(0077); 
#endif
}

int main() {
    // 1. Bootstrap Logging (Console for now, File later)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("console", console_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info); // Change to 'debug' for dev
    spdlog::set_pattern("[%H:%M:%S %z] [%^%l%$] [thread %t] %v");

    spdlog::info("MediaStream Server V2.0 - Starting up...");

    // 2. Security Handshake
    enforce_security_context();

    // 3. Register Signal Handlers
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // kill <pid>

    // 4. Oat++ Init
    oatpp::base::Environment::init();

    // 5. Main Execution Block
    // Wrap in try-catch to prevent "terminate called without an active exception" crashes.
    try {

        // 1. The Database
        spdlog::info("Booting Database...");
        auto db = std::make_shared<media::database::Database>("./media.db");
        db->initialize();

        // 2. The Core (Domain)
        spdlog::info("Booting Core...");
        // Resolved to an absolute path so the storage location never depends on
        // the working directory the service was launched from. Override with
        // MEDIASTREAM_DOWNLOAD_DIR (e.g. Environment= in the systemd unit).
        const char* dl_env = std::getenv("MEDIASTREAM_DOWNLOAD_DIR");
        const auto download_dir =
            std::filesystem::absolute(dl_env && *dl_env ? dl_env : "./downloads");
        spdlog::info("Download directory: {}", download_dir.string());
        auto engine = std::make_shared<media::core::TorrentEngine>(download_dir);

        // 3. The Services
        spdlog::info("Booting Services...");
        auto discovery = std::make_shared<media::services::ContentDiscoveryManager>();

        // 4. The API (Interface)
        spdlog::info("Booting API...");
        media::api::HttpServer api_server(engine, discovery, db);
        api_server.start();

        // 5. The Keep-Alive Loop
        spdlog::info("System Online.");
        while (g_keep_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 5. Shutdown
        spdlog::info("Shutting down...");
        api_server.stop(); // Cleanly stop the web server
        // engine destructor runs automatically here

    } catch (const std::exception& ex) {
        spdlog::critical("Fatal Error: {}", ex.what());
    }

    oatpp::base::Environment::destroy();

    spdlog::info("Goodbye.");
    return EXIT_SUCCESS;
}