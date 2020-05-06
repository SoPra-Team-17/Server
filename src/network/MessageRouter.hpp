#ifndef SERVER017_MESSAGEROUTER_HPP
#define SERVER017_MESSAGEROUTER_HPP

#include <nlohmann/json.hpp>
#include <Server/WebSocketServer.hpp>
#include <utility>
#include <spdlog/spdlog.h>
#include <network/messages/Hello.hpp>
#include <set>

/**
 * The MessageRouter holds a websocket::network::WebSocketServer and manages and enumerates connections.
 */
class MessageRouter {
    public:

        using connectionPtr = std::shared_ptr<websocket::network::Connection>;
        using connection = std::pair<connectionPtr, std::optional<spy::util::UUID>>;
        using connectionMap = std::vector<connection>;

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

        /**
         * Sends a message to a specific client.
         * Message field MessageContainer::clientId will be set to the value of \p client
         * @param client message recipient
         */
        template<typename MessageType>
        void sendMessage(spy::util::UUID client, MessageType message) {
            message.setClientId(client);
            nlohmann::json serializedMessage = message;
            auto &con = connectionFromUUID(client);
            con.first->send(serializedMessage.dump());
        }

        /**
         * Assigns a UUID to a specific connection
         * @param id         UUID of client
         * @param connection Pointer to connection
         */
        void registerUUIDforConnection(const spy::util::UUID &id, const connectionPtr &connection);

    private:
        websocket::network::WebSocketServer server;

        // Active connections, including spectators.
        connectionMap activeConnections;

        connection &connectionFromPtr(const connectionPtr &con);

        connection &connectionFromUUID(const spy::util::UUID &id);

        /**
         * New-connection-listener, called by server
         */
        void connectListener(const connectionPtr& newConnection);

        /**
         * Connection-close-listener, called by server
         */
        void disconnectListener(const connectionPtr& closedConnection);

        /**
         * Receive-listener, called by each connection
         */
        void receiveListener(const connectionPtr& connection, const std::string& message);

        const websocket::util::Listener<spy::network::messages::Hello, connectionPtr> helloListener;
};

#endif //SERVER017_MESSAGEROUTER_HPP
