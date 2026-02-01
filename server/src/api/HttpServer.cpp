#include "mediastream/api/HttpServer.hpp"
#include "mediastream/api/TorrentController.hpp"

// Oat++ Headers
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"

// SWAGGER DISABLED FOR PHASE 3 TESTING
// #include "oatpp-swagger/Controller.hpp"

#include <spdlog/spdlog.h>

namespace media::api {

    HttpServer::HttpServer(std::shared_ptr<media::core::TorrentEngine> engine)
        : m_engine(engine), m_should_run(false) 
    {}

    HttpServer::~HttpServer() {
        stop();
    }

    void HttpServer::start() {
        if (m_should_run) return;
        m_should_run = true;
        m_server_thread = std::thread(&HttpServer::run_internal, this);
        spdlog::info("HttpServer started on background thread.");
    }

    void HttpServer::stop() {
        if (!m_should_run) return;

        spdlog::info("Stopping HttpServer...");
        m_should_run = false; 
        
        if (m_server_thread.joinable()) {
            m_server_thread.detach(); 
        }
        spdlog::info("HttpServer stopped.");
    }

    void HttpServer::run_internal() {
        try {
            auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
            auto router = oatpp::web::server::HttpRouter::createShared();
            auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});

            // 1. Register API Controller
            auto torrentController = std::make_shared<TorrentController>(objectMapper, m_engine);
            router->addController(torrentController);

            // 2. Swagger Disabled (Bypassing version conflict)
            // We will verify the API using raw CURL commands instead.
            
            // 3. Create Server
            oatpp::network::Server server(connectionProvider, 
                                          oatpp::web::server::HttpConnectionHandler::createShared(router));
            
            spdlog::info("REST API listening on port 8000...");
            
            // 4. Run (Blocking)
            server.run(); 

        } catch (const std::exception& e) {
            spdlog::critical("HttpServer Crash: {}", e.what());
        }
    }

} // namespace media::api