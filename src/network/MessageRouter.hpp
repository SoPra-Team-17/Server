#ifndef SERVER017_MESSAGEROUTER_HPP
#define SERVER017_MESSAGEROUTER_HPP

#include <nlohmann/json.hpp>
#include <Server/WebSocketServer.hpp>
#include <utility>

/**
 * The MessageRouter holds a websocket::network::WebSocketServer and manages and enumerates connections.
 */
class MessageRouter {
public:
    MessageRouter(uint16_t closedConnection, std::string protocol);

    template<typename MessageType>
    void broadcastMessage(MessageType message) {
        nlohmann::json serializedMessage = message;
        server.broadcast(serializedMessage.dump());
    }

    template<typename MessageType>
    void sendMessage(int client, MessageType message) {
        if (activeConnections.find(client) == activeConnections.end()) {
            spdlog::error("Client Nr. {} does not exist in map of known connections", client);
            throw std::invalid_argument("Invalid client nr. " + std::to_string(client));
        }
        nlohmann::json serializedMessage = message;
        activeConnections.at(client)->send(message.dump());
    }


private:
    websocket::network::WebSocketServer server;
    std::map<int, std::shared_ptr<websocket::network::Connection>> activeConnections;

    /**
     * New-connection-listener, called by server
     */
    void connectListener(std::shared_ptr<websocket::network::Connection> newConnection);

    /**
     * Connection-close-listener, called by server
     */
    void disconnectListener(std::shared_ptr<websocket::network::Connection> closedConnection);
};

#endif //SERVER017_MESSAGEROUTER_HPP
