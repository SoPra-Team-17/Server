#ifndef SERVER017_MESSAGEROUTER_HPP
#define SERVER017_MESSAGEROUTER_HPP

#include <nlohmann/json.hpp>
#include <Server/WebSocketServer.hpp>
#include <utility>
#include <spdlog/spdlog.h>
#include <network/messages/Hello.hpp>

/**
 * The MessageRouter holds a websocket::network::WebSocketServer and manages and enumerates connections.
 */
class MessageRouter {
    public:
        MessageRouter(uint16_t closedConnection, std::string protocol);

        template<typename MessageType>
        void broadcastMessage(MessageType message) {
            nlohmann::json serializedMessage = message;
            spdlog::info("Broadcasting message: " + serializedMessage.dump());
            server.broadcast(serializedMessage.dump());
        }

        template<typename T>
        void addHelloListener(T l) {
            spdlog::info("Adding HelloListener to MessageRouter.");
            helloListener.subscribe(l);
        }

        template<typename MessageType>
        void sendMessage(int client, MessageType message) {
            if (activeConnections.find(client) == activeConnections.end()) {
                spdlog::error("Client Nr. {} does not exist in map of known connections", client);
                throw std::invalid_argument("Invalid client nr. " + std::to_string(client));
            }
            nlohmann::json serializedMessage = message;
            activeConnections.at(client)->send(serializedMessage.dump());
        }

    private:
        websocket::network::WebSocketServer server;

        // Active connections, including spectators. Special values: Keys 0 and 1 (Player::one, Player::two)
        std::map<int, std::shared_ptr<websocket::network::Connection>> activeConnections;

        /**
         * New-connection-listener, called by server
         */
        void connectListener(std::shared_ptr<websocket::network::Connection> newConnection);

        /**
         * Connection-close-listener, called by server
         */
        void disconnectListener(std::shared_ptr<websocket::network::Connection> closedConnection);

        /**
         * Receive-listener, called by each connection
         */
        void receiveListener(int connectionIndex,
                             std::shared_ptr<websocket::network::Connection> connection,
                             std::string message);

        const websocket::util::Listener<spy::network::messages::Hello> helloListener;
};

#endif //SERVER017_MESSAGEROUTER_HPP
