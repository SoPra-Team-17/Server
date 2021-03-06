#ifndef SERVER017_MESSAGEROUTER_HPP
#define SERVER017_MESSAGEROUTER_HPP

#include <nlohmann/json.hpp>
#include <Server/WebSocketServer.hpp>
#include <utility>
#include <spdlog/spdlog.h>
#include <set>
#include <network/messages/Hello.hpp>
#include <network/messages/Reconnect.hpp>
#include <network/messages/ItemChoice.hpp>
#include <network/messages/EquipmentChoice.hpp>
#include <network/messages/GameOperation.hpp>
#include <network/messages/GameLeave.hpp>
#include <network/messages/RequestGamePause.hpp>
#include <network/messages/RequestMetaInformation.hpp>
#include <network/messages/RequestReplay.hpp>
#include <util/UUIDNotFoundException.hpp>

/**
 * The MessageRouter holds a websocket::network::WebSocketServer and manages and enumerates connections.
 */
class MessageRouter {
    public:
        using connectionPtr = std::shared_ptr<websocket::network::Connection>;
        using connection = std::pair<connectionPtr, std::optional<spy::util::UUID>>;
        using connectionMap = std::vector<connection>;

        MessageRouter(uint16_t port, std::string protocol);

        template<typename MessageType>
        void broadcastMessage(MessageType message) {
            for (const auto &[ptr, uuid] :activeConnections) {
                if (!uuid.has_value()) {
                    spdlog::warn("Broadcasting message while there is unregistered connection");
                    continue;
                }
                sendMessage(uuid.value(), message);
            }
        }

        template<typename T>
        void addHelloListener(T l) {
            helloListener.subscribe(l);
        }

        template<typename T>
        void addReconnectListener(T l) {
            reconnectListener.subscribe(l);
        }

        template<typename T>
        void addItemChoiceListener(T l) {
            itemChoiceListener.subscribe(l);
        }

        template<typename T>
        void addEquipmentChoiceListener(T l) {
            equipmentChoiceListener.subscribe(l);
        }

        template<typename T>
        void addGameOperationListener(T l) {
            gameOperationListener.subscribe(l);
        }

        template<typename T>
        void addGameLeaveListener(T l) {
            gameLeaveListener.subscribe(l);
        }

        template<typename T>
        void addPauseRequestListener(T l) {
            pauseRequestListener.subscribe(l);
        }

        template<typename T>
        void addMetaInformationRequestListener(T l) {
            metaInformationRequestListener.subscribe(l);
        }

        template<typename T>
        void addReplayRequestListener(T l) {
            replayRequestListener.subscribe(l);
        }

        template<typename T>
        void addDisconnectListener(T l) {
            clientDisconnectListener.subscribe(l);
        }

        bool isConnected(const spy::util::UUID &id) const;


        /**
         * Sends a message to a specific client.
         * Message field MessageContainer::clientId will be set to the value of \p client
         * @param client message recipient
         */
        template<typename MessageType>
        void sendMessage(spy::util::UUID client, MessageType message) {
            message.setClientId(client);
            sendMessage(message);
        }

        /**
         * Sends a message to the client specified in the message
         */
        template<typename MessageType>
        void sendMessage(MessageType message) {
            try {
                auto &con = connectionFromUUID(message.getClientId());
                sendMessage(con.first, message);
            } catch (const UUIDNotFoundException &e) {
                spdlog::warn("UUIDNotFoundException: {}", e.what());
                spdlog::warn("Tried sending message to UUID {}, but it's not found in connection list.",
                             message.getClientId());
                spdlog::debug("Active connections: ");
                for (const auto &[_, optID]: activeConnections) {
                    spdlog::debug(optID.value_or(spy::util::UUID{}));
                }
            }
        }

        /**
         * Sends a message to a specific connection.
         */
        template<typename MessageType>
        void sendMessage(connectionPtr connectionPtr, MessageType message) {
            nlohmann::json serializedMessage = message;
            spdlog::trace("Sending message: {}", serializedMessage.dump());
            connectionPtr->send(serializedMessage.dump());
        }

        /**
         * Assigns a UUID to a specific connection
         * @param id         UUID of client
         * @param connection Pointer to connection
         */
        void registerUUIDforConnection(const spy::util::UUID &id, const connectionPtr &connection);

        void clearConnections();

        void closeConnection(const spy::util::UUID &id);

    private:
        websocket::network::WebSocketServer server;

        // Active connections, including spectators.
        connectionMap activeConnections;

        connection &connectionFromPtr(const connectionPtr &con);

        connection &connectionFromUUID(const spy::util::UUID &id);

        /**
         * New-connection-listener, called by server
         */
        void connectListener(const connectionPtr &newConnection);

        /**
         * Connection-close-listener, called by server
         */
        void disconnectListener(const connectionPtr &closedConnection);

        /**
         * Receive-listener, called by each connection
         */
        void receiveListener(const connectionPtr &connection, const std::string &message);

        const websocket::util::Listener<spy::network::messages::Hello, connectionPtr> helloListener;
        const websocket::util::Listener<spy::network::messages::Reconnect, connectionPtr> reconnectListener;
        const websocket::util::Listener<spy::network::messages::ItemChoice> itemChoiceListener;
        const websocket::util::Listener<spy::network::messages::EquipmentChoice> equipmentChoiceListener;
        const websocket::util::Listener<spy::network::messages::GameOperation> gameOperationListener;
        const websocket::util::Listener<spy::network::messages::GameLeave> gameLeaveListener;
        const websocket::util::Listener<spy::network::messages::RequestGamePause> pauseRequestListener;
        const websocket::util::Listener<spy::network::messages::RequestMetaInformation> metaInformationRequestListener;
        const websocket::util::Listener<spy::network::messages::RequestReplay> replayRequestListener;

        const websocket::util::Listener<spy::util::UUID> clientDisconnectListener;
};

#endif //SERVER017_MESSAGEROUTER_HPP
