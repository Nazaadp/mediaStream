#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <filesystem>

// Third-party
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// Internal Domain
#include "mediastream/core/TorrentEngine.hpp"

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

    // 4. Main Execution Block
    // Wrap in try-catch to prevent "terminate called without an active exception" crashes.
    try {
        // CONFIGURATION (Hardcoded for Phase 1, move to config.json later)
        const std::filesystem::path download_dir = "./downloads";
        
        spdlog::info("Initializing Torrent Engine...");
        media::core::TorrentEngine engine(download_dir);

        spdlog::info("System Ready. Waiting for commands.");

        // --- TEST CODE (Uncomment to test Phase 1) ---
        // engine.addMagnet("magnet:?xt=urn:btih:...");
        // ---------------------------------------------

        // 5. The "Event Loop"
        // Since we don't have a REST API listener yet (Phase 3), 
        // we manually keep the main thread alive.
        while (g_keep_running) {
            // Wake up every 1 second to check status or signals
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Optional: Periodic Status Logging
            // auto status = engine.getSessionStatus();
            // spdlog::debug("Active Torrents: {}", status.size());
        }

    } catch (const std::exception& ex) {
        spdlog::critical("Unrecoverable Error: {}", ex.what());
        return EXIT_FAILURE;
    } catch (...) {
        spdlog::critical("Unknown Error occurred during execution.");
        return EXIT_FAILURE;
    }

    // 6. Graceful Shutdown
    // When the loop breaks, the 'engine' variable goes out of scope.
    // The Destructor (~TorrentEngine) fires automatically, saving state/pausing sessions.
    spdlog::info("MediaStream Server shutdown complete. Goodbye.");
    
    return EXIT_SUCCESS;
}