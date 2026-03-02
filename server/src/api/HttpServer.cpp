#include "mediastream/api/HttpServer.hpp"
#include "mediastream/api/TorrentController.hpp"
#include "mediastream/api/StreamController.hpp"
#include "mediastream/api/DiscoveryController.hpp"
#include "mediastream/api/UserController.hpp"
#include "mediastream/api/WebSocketController.hpp"
#include "mediastream/services/ContentDiscovery.hpp"

// Oat++ Headers
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/interceptor/ResponseInterceptor.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp-websocket/ConnectionHandler.hpp"

// SWAGGER DISABLED FOR PHASE 3 TESTING
// #include "oatpp-swagger/Controller.hpp"

#include <spdlog/spdlog.h>

namespace media::api {

    HttpServer::HttpServer(std::shared_ptr<media::core::TorrentEngine> engine,
                           std::shared_ptr<media::services::ContentDiscoveryManager> discovery,
                           std::shared_ptr<media::database::Database> db)
        : m_engine(engine), m_discovery(discovery), m_db(db), m_should_run(false) 
    {}

    HttpServer::~HttpServer() {
        stop();
    }

    // Custom CORS Interceptor for Oatpp v1.3.0 where AllowCorsGlobal doesn't exist natively
    class CorsInterceptor : public oatpp::web::server::interceptor::ResponseInterceptor {
    public:
        std::shared_ptr<OutgoingResponse> intercept(const std::shared_ptr<IncomingRequest>& request,
                                                    const std::shared_ptr<OutgoingResponse>& response) override {
            (void)request; // Suppress unused parameter warning
            response->putHeaderIfNotExists("Access-Control-Allow-Origin", "http://localhost:1420");
            response->putHeaderIfNotExists("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
            response->putHeaderIfNotExists("Access-Control-Allow-Headers", "DNT, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type, Range, Authorization");
            response->putHeaderIfNotExists("Access-Control-Expose-Headers", "Content-Range, Accept-Ranges, Content-Length");
            response->putHeaderIfNotExists("Access-Control-Max-Age", "1728000");
            return response;
        }
    };

    void HttpServer::start() {
        if (m_should_run) return;
        m_should_run = true;
        m_server_thread = std::thread(&HttpServer::run_internal, this);
        m_ws_broadcaster_thread = std::thread(&HttpServer::run_ws_broadcaster, this);
        spdlog::info("HttpServer started on background thread.");
    }

    void HttpServer::stop() {
        if (!m_should_run) return;

        spdlog::info("Stopping HttpServer...");
        m_should_run = false; 
        
        if (m_server_thread.joinable()) {
            m_server_thread.detach(); 
        }
        if (m_ws_broadcaster_thread.joinable()) {
            m_ws_broadcaster_thread.detach();
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

            auto userController = std::make_shared<UserController>(objectMapper, m_db);
            router->addController(userController);

            auto websocketConnectionHandler = oatpp::websocket::ConnectionHandler::createShared();
            m_ws_controller = WebSocketController::createShared(objectMapper, websocketConnectionHandler);
            router->addController(m_ws_controller);

            // 2. Swagger Disabled (Bypassing version conflict)
            // We will verify the API using raw CURL commands instead.
            
            // 3. Create Connection Handler and add custom CORS interceptors
            auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
            connectionHandler->addResponseInterceptor(std::make_shared<CorsInterceptor>());

            // 4. Create Server
            oatpp::network::Server server(connectionProvider, connectionHandler);
            
            spdlog::info("REST API listening on port 8000...");
            
            // 4. Run (Blocking)
            server.run(); 

        } catch (const std::exception& e) {
            spdlog::critical("HttpServer Crash: {}", e.what());
        }
    }

    void HttpServer::run_ws_broadcaster() {
        auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        oatpp::String last_json = "";

        while (m_should_run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            try {
                if (m_ws_controller) {
                    auto engine_status = m_engine->getSessionStatus();
                    if (engine_status.empty()) {
                        if (last_json != "[]") {
                            last_json = "[]";
                            m_ws_controller->broadcastStatus("[]");
                        }
                        continue; 
                    }

                    auto response_list = oatpp::Vector<oatpp::Object<TorrentStatusDto>>::createShared();
                    for (const auto& item : engine_status) {
                        auto dto = TorrentStatusDto::createShared();
                        dto->info_hash = item.info_hash;
                        dto->name = item.name;
                        dto->progress = item.progress;
                        dto->state = item.state;
                        dto->download_rate = item.download_rate;
                        response_list->push_back(dto);
                    }
                    
                    auto json = objectMapper->writeToString(response_list);
                    if (json != last_json) {
                        last_json = json;
                        m_ws_controller->broadcastStatus(json);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("WebSocket Broadcaster Error: {}", e.what());
                // Prevent thread crash. Assume last_json is dirty, will retry next sec.
            } catch (...) {
                spdlog::error("WebSocket Broadcaster Unknown Error");
            }
        }
    }

} // namespace media::api