#include "mediastream/api/HttpServer.hpp"
#include "mediastream/api/TorrentController.hpp"
#include "mediastream/api/StreamController.hpp"
#include "mediastream/api/DiscoveryController.hpp"
#include "mediastream/services/ContentDiscovery.hpp"

// Oat++ Headers
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/interceptor/AllowCorsGlobal.hpp"
#include "oatpp/web/server/interceptor/AllowOptionsGlobal.hpp"

// SWAGGER DISABLED FOR PHASE 3 TESTING
// #include "oatpp-swagger/Controller.hpp"

#include <spdlog/spdlog.h>

namespace media::api {

    HttpServer::HttpServer(std::shared_ptr<media::core::TorrentEngine> engine,
                           std::shared_ptr<media::services::ContentDiscoveryManager> discovery)
        : m_engine(engine), m_discovery(discovery), m_should_run(false) 
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

            /*
            
                When a device connects to your server, that C++ Interceptor will read the incoming Origin: header.

                If the Origin is "http://localhost:1420" (Your PC), your C++ server replies: "Access-Control-Allow-Origin: http://localhost:1420".
                If the Origin is "tauri://localhost" (Your Android), your C++ server replies: "Access-Control-Allow-Origin: tauri://localhost".
                If the Origin is "http://evil-hacker.com", your C++ server replies with nothing, and the hacker's browser blocks the connection!
                            
            */


            auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});

            // 1. Register API Controllers
            auto torrentController = std::make_shared<TorrentController>(objectMapper, m_engine);
            router->addController(torrentController);

            auto streamController = std::make_shared<StreamController>(objectMapper, m_engine);
            router->addController(streamController);

            auto discoveryController = std::make_shared<DiscoveryController>(objectMapper, m_discovery);
            router->addController(discoveryController);

            // 2. Swagger Disabled (Bypassing version conflict)
            // We will verify the API using raw CURL commands instead.
            
            // 3. Create Connection Handler and add CORS interceptors
            auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
            
            connectionHandler->addRequestInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowOptionsGlobal>());
            connectionHandler->addResponseInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowCorsGlobal>(
                "http://localhost:1420", 
                "GET, POST, OPTIONS, PUT, DELETE", 
                "DNT, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type, Range, Authorization"
            ));

            // 4. Create Server
            oatpp::network::Server server(connectionProvider, connectionHandler);
            
            spdlog::info("REST API listening on port 8000...");
            
            // 4. Run (Blocking)
            server.run(); 

        } catch (const std::exception& e) {
            spdlog::critical("HttpServer Crash: {}", e.what());
        }
    }

} // namespace media::api