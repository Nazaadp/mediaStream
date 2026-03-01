#pragma once

#include "oatpp-websocket/AsyncWebSocket.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
#include <unordered_set>

namespace media::api {

/**
 * Custom WebSocketListener to handle individual connections
 */
class StatusWebSocketListener : public oatpp::websocket::AsyncWebSocket::Listener {
private:
    std::shared_ptr<oatpp::websocket::AsyncWebSocket> m_socket;
    // We can use a reference to the global manager to unregister on disconnect
    std::function<void(StatusWebSocketListener*)> m_onDisconnect;

public:
    StatusWebSocketListener(const std::function<void(StatusWebSocketListener*)>& onDisconnect)
        : m_onDisconnect(onDisconnect) 
    {}

    // Triggered when the socket connects
    oatpp::coro::CoroutineStarter onPing(const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket, const oatpp::String& message) override {
        return socket->sendPongAsync(message);
    }

    oatpp::coro::CoroutineStarter onPong(const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket, const oatpp::String& message) override {
        (void)socket;
        (void)message;
        return nullptr;
    }

    oatpp::coro::CoroutineStarter onClose(const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket, v_uint16 code, const oatpp::String& message) override {
        (void)socket;
        (void)code;
        (void)message;
        spdlog::info("WebSocket connection closed.");
        m_onDisconnect(this);
        return nullptr;
    }

    oatpp::coro::CoroutineStarter readMessage(const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket, v_uint8 opcode, p_char8 data, oatpp::v_io_size size) override {
        (void)socket;
        (void)opcode;
        (void)data;
        (void)size;
        // Read messages from client (not strictly needed right now since client only listens)
        return nullptr;
    }

    // Custom method to push data to the client
    void sendMessage(const oatpp::String& msg) {
        if(m_socket) {
            // Note: oatpp Websocket is thread safe for sending messages in async mode,
            // but we must be careful. Best practice is locking or async dispatcher.
            m_socket->sendOneFrameTextAsync(msg);
        }
    }

    void setSocket(const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket) {
        m_socket = socket;
    }
};


#include OATPP_CODEGEN_BEGIN(ApiController)

/**
 * Controller with WebSocket endpoint
 */
class WebSocketController : public oatpp::web::server::api::ApiController {
private:
    std::mutex m_clientsMutex;
    std::unordered_set<StatusWebSocketListener*> m_clients;

    void registerClient(StatusWebSocketListener* client) {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.insert(client);
    }

    void unregisterClient(StatusWebSocketListener* client) {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.erase(client);
    }

public:
    WebSocketController(const std::shared_ptr<ObjectMapper>& objectMapper)
        : oatpp::web::server::api::ApiController(objectMapper)
    {}

    static std::shared_ptr<WebSocketController> createShared(const std::shared_ptr<ObjectMapper>& objectMapper) {
        return std::make_shared<WebSocketController>(objectMapper);
    }

    // Endpoint that handles WebSocket upgrades
    ENDPOINT("GET", "/api/v1/ws/status", wsStatus) {
        return oatpp::websocket::Handshaker::serversideHandshake(
            getRequestHeaders(),
            [this](const std::shared_ptr<oatpp::websocket::AsyncWebSocket>& socket) {
                // Connection achieved! Create a listener.
                auto listener = std::make_shared<StatusWebSocketListener>(
                    [this](StatusWebSocketListener* c) { this->unregisterClient(c); }
                );
                
                listener->setSocket(socket);
                this->registerClient(listener.get());
                
                socket->setListener(listener);
                spdlog::info("New WebSocket client connected for /api/v1/ws/status.");
            }
        );
    }

    // Broadcast message to all connected clients
    void broadcastStatus(const oatpp::String& message) {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto* client : m_clients) {
            client->sendMessage(message);
        }
    }
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace media::api
