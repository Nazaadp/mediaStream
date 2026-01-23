#include "mediastream/api/HttpServer.hpp"
#include "mediastream/api/TorrentController.hpp"

// Oat++ Headers
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp-swagger/Controller.hpp"

// Logging
#include <spdlog/spdlog.h>

namespace media::api {

    HttpServer::HttpServer(std::shared_ptr<media::core::TorrentEngine> engine)
        : m_engine(engine), m_should_run(false) 
    {}

    HttpServer::~HttpServer() {
        stop();
    }

    void HttpServer::start() {
        if (m_should_run) return; // Already running
        
        m_should_run = true;
        // Launch the server loop in a background thread
        m_server_thread = std::thread(&HttpServer::run_internal, this);
        
        spdlog::info("HttpServer started on background thread.");
    }

    void HttpServer::stop() {
        if (!m_should_run) return;

        spdlog::info("Stopping HttpServer...");
        m_should_run = false; // The loop in run_internal will see this and break
        
        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
        spdlog::info("HttpServer stopped.");
    }

    void HttpServer::run_internal() {
        try {
            // 1. Setup Components
            auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
            auto router = oatpp::web::server::HttpRouter::createShared();
            auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});

            // 2. Register Controllers
            auto torrentController = std::make_shared<TorrentController>(objectMapper, m_engine);
            router->addController(torrentController);

            // 3. Register Swagger
            oatpp::swagger::DocumentInfo docInfo;
            docInfo.title = "MediaStream API";
            docInfo.version = "2.0";
            auto swaggerController = oatpp::swagger::Controller::createShared(docInfo);
            swaggerController->addEndpointsToRouter(router);

            // 4. Create Server
            oatpp::network::Server server(connectionProvider, 
                                          oatpp::web::server::HttpConnectionHandler::createShared(router));
            
            spdlog::info("REST API listening on port 8000...");

            // 5. The Loop
            // We verify m_should_run every 100ms so we can shut down cleanly
            while (m_should_run) {
                if (!server.run(true)) { 
                    // .run(true) processes one connection/tick and returns
                    // If it returns false, the socket is broken
                    break; 
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Cleanup
            server.stop();
            connectionProvider->stop();

        } catch (const std::exception& e) {
            spdlog::critical("HttpServer Crash: {}", e.what());
        }
    }

} // namespace media::api