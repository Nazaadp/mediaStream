#pragma once

#include "oatpp-websocket/WebSocket.hpp"
#include "oatpp-websocket/ConnectionHandler.hpp"
#include "oatpp-websocket/Handshaker.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
#include <unordered_set>

namespace media::api {

/**
 * Custom WebSocketListener to handle individual connections (Synchronous)
 */
class StatusWebSocketListener : public oatpp::websocket::WebSocket::Listener {
    std::mutex* m_clientsMutex;
    std::unordered_set<StatusWebSocketListener*>* m_clients;
    oatpp::websocket::WebSocket* m_socket; 
    std::mutex m_socketMutex; // Prevents concurrent access to m_socket

public:
    StatusWebSocketListener(std::mutex* clientsMutex, std::unordered_set<StatusWebSocketListener*>* clients)
        : m_clientsMutex(clientsMutex), m_clients(clients), m_socket(nullptr)
    {
        std::lock_guard<std::mutex> lock(*m_clientsMutex);
        m_clients->insert(this);
    }

    ~StatusWebSocketListener() {
        std::lock_guard<std::mutex> lock(*m_clientsMutex);
        m_clients->erase(this);
    }

    void onPing(const oatpp::websocket::WebSocket& socket, const oatpp::String& message) override {
        // Send pong back
        socket.sendPong(message);
    }

    void onPong(const oatpp::websocket::WebSocket& socket, const oatpp::String& message) override {
        (void)socket;
        (void)message;
    }

    void onClose(const oatpp::websocket::WebSocket& socket, v_uint16 code, const oatpp::String& message) override {
        (void)socket;
        (void)code;
        (void)message;
        spdlog::info("WebSocket connection closed.");
    }

    void readMessage(const oatpp::websocket::WebSocket& socket, v_uint8 opcode, p_char8 data, oatpp::v_io_size size) override {
        (void)socket;
        (void)opcode;
        (void)data;
        (void)size;
        // Read messages from client (not strictly needed right now since client only listens)
    }

    // Capture the socket once connected
    void setSocket(oatpp::websocket::WebSocket* socket) {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        m_socket = socket;
    }

    // Called when the connection handler Destroys the socket
    void invalidateSocket() {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        m_socket = nullptr;
    }

    // Custom method to push data to the client
    void sendMessage(const oatpp::String& msg) {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if(m_socket) {
            try {
                m_socket->sendOneFrameText(msg);
            } catch (...) {
                // Ignore exceptions, the socket is likely closed or broken
                m_socket = nullptr;
            }
        }
    }
};

/**
 * Instance listener that creates a new StatusWebSocketListener per connection
 */
class WSInstanceListener : public oatpp::websocket::ConnectionHandler::SocketInstanceListener {
private:
    std::mutex* m_clientsMutex;
    std::unordered_set<StatusWebSocketListener*>* m_clients;

public:
    WSInstanceListener(std::mutex* clientsMutex, std::unordered_set<StatusWebSocketListener*>* clients)
        : m_clientsMutex(clientsMutex), m_clients(clients)
    {}

    /**
     *  This method is called when socket is created
     */
    void onAfterCreate(const oatpp::websocket::WebSocket& socket, const std::shared_ptr<const ParameterMap>& params) override {
        (void)socket;
        (void)params;
        auto listener = std::make_shared<StatusWebSocketListener>(m_clientsMutex, m_clients);
        // We have to cast const away because Oat++ 1.3.0 passes 'const WebSocket&', but 'setListener' requires non-const or is called internally
        // Actually, oatpp 1.3.0 does: const_cast<WebSocket*>(&socket)->setListener(listener);
        auto* mutableSocket = const_cast<oatpp::websocket::WebSocket*>(&socket);
        listener->setSocket(mutableSocket);
        mutableSocket->setListener(listener);
        spdlog::info("New WebSocket client connected for /api/v1/ws/status.");
    }

    void onBeforeDestroy(const oatpp::websocket::WebSocket& socket) override {
        // Safe tear down
        auto* mutableSocket = const_cast<oatpp::websocket::WebSocket*>(&socket);
        auto listener = mutableSocket->getListener();
        if (listener) {
             auto myListener = std::static_pointer_cast<StatusWebSocketListener>(listener);
             myListener->invalidateSocket();
        }
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
    std::shared_ptr<oatpp::websocket::ConnectionHandler> m_websocketConnectionHandler;

public:
    WebSocketController(const std::shared_ptr<ObjectMapper>& objectMapper, 
                        const std::shared_ptr<oatpp::websocket::ConnectionHandler>& websocketConnectionHandler)
        : oatpp::web::server::api::ApiController(objectMapper)
        , m_websocketConnectionHandler(websocketConnectionHandler)
    {
        m_websocketConnectionHandler->setSocketInstanceListener(
            std::make_shared<WSInstanceListener>(&m_clientsMutex, &m_clients)
        );
    }

    static std::shared_ptr<WebSocketController> createShared(const std::shared_ptr<ObjectMapper>& objectMapper,
                                                             const std::shared_ptr<oatpp::websocket::ConnectionHandler>& websocketConnectionHandler) {
        return std::make_shared<WebSocketController>(objectMapper, websocketConnectionHandler);
    }

    // Endpoint that handles WebSocket upgrades
    ENDPOINT("GET", "/api/v1/ws/status", wsStatus, REQUEST(std::shared_ptr<IncomingRequest>, request)) {
        return oatpp::websocket::Handshaker::serversideHandshake(request->getHeaders(), m_websocketConnectionHandler);
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
