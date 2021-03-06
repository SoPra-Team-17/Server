
#include <utility>
#include <algorithm>
#include <network/messages/Error.hpp>
#include "MessageRouter.hpp"
#include "spdlog/fmt/ostr.h"
#include "util/UUIDNotFoundException.hpp"

MessageRouter::MessageRouter(uint16_t port, std::string protocol) : server{port, std::move(protocol)} {
    server.connectionListener.subscribe(
            [this](const connectionPtr &newConnection) {
                connectListener(newConnection);
            });
    server.closeListener.subscribe(
            [this](const connectionPtr &closedConnection) {
                disconnectListener(closedConnection);
            });
}

void MessageRouter::connectListener(const MessageRouter::connectionPtr &newConnection) {
    spdlog::info("New client connected");

    // Connection does not have UUID yet
    activeConnections.emplace_back(newConnection, std::nullopt);

    newConnection->receiveListener.subscribe([this, newConnection](const std::string &message) {
        receiveListener(newConnection, message);
    });
}

void MessageRouter::disconnectListener(const MessageRouter::connectionPtr &closedConnection) {
    spdlog::info("Router: client disconnect");
    std::optional<spy::util::UUID> connectionUUID;
    try {
        const auto &foundConnection = connectionFromPtr(closedConnection);
        connectionUUID = foundConnection.second;
    } catch (const std::invalid_argument &e) {
        spdlog::info("Not registered connection closed. (Exception: {})", e.what());
        return;
    }

    if (connectionUUID.has_value()) {
        spdlog::info("Connection {} closed.", connectionUUID.value());
    } else {
        spdlog::info("Connection without UUID closed.");
    }

    auto con = std::find_if(activeConnections.begin(), activeConnections.end(), [closedConnection](const auto &c) {
        return c.first == closedConnection;
    });
    if (con == activeConnections.end()) {
        spdlog::error("Connection {} not found in list using pointer.", connectionUUID.value_or(spy::util::UUID{}));
        return;
    }

    activeConnections.erase(con);

    if (connectionUUID.has_value()) {
        clientDisconnectListener(connectionUUID.value());
    }
}

void MessageRouter::receiveListener(const MessageRouter::connectionPtr &connectionPtr, const std::string &message) {
    std::optional<spy::util::UUID> connectionId = std::nullopt;
    try {
        const auto &con = connectionFromPtr(connectionPtr);
        connectionId = con.second;
    } catch (const std::invalid_argument &) {
        spdlog::warn("Received message from kicked client");
        return;
    }

    spdlog::trace("Received message from client {} : {}", connectionId.value_or(spy::util::UUID{}), message);
    nlohmann::json messageJson;
    try {
        messageJson = nlohmann::json::parse(message);
        auto messageContainer = messageJson.get<spy::network::MessageContainer>();

        //check if client sends with his own UUID
        if (messageContainer.getType() != spy::network::messages::MessageTypeEnum::HELLO
            and messageContainer.getType() != spy::network::messages::MessageTypeEnum::RECONNECT) {
            // All messages other than HELLO and RECONNECT require that the client is already registered.
            // -> connectionId should have been found and be equal to clientId in message.

            if (not connectionId.has_value()) {
                spdlog::error("Received message from unregistered client that is not HELLO or RECONNECT."
                              "Not handling message.");
                return;
            }

            if (connectionId.value() != messageContainer.getClientId()) {
                spdlog::warn("Client {} sent a message with false uuid: {}. Correcting UUID and handling message.",
                             connectionId.value(),
                             messageContainer.getClientId());
                messageJson.at("clientId") = connectionId.value(); // correct the uuid
            }
        }

        switch (messageContainer.getType()) {
            case spy::network::messages::MessageTypeEnum::INVALID:
                spdlog::error("Received message with invalid type: " + message);
                return;
            case spy::network::messages::MessageTypeEnum::HELLO:
                spdlog::debug("MessageRouter received Hello message.");
                helloListener(messageJson.get<spy::network::messages::Hello>(), connectionPtr);
                return;
            case spy::network::messages::MessageTypeEnum::RECONNECT:
                spdlog::debug("MessageRouter received Reconnect message.");
                reconnectListener(messageJson.get<spy::network::messages::Reconnect>(), connectionPtr);
                return;
            case spy::network::messages::MessageTypeEnum::ITEM_CHOICE:
                spdlog::debug("MessageRouter received ItemChoice message.");
                itemChoiceListener(messageJson.get<spy::network::messages::ItemChoice>());
                return;
            case spy::network::messages::MessageTypeEnum::EQUIPMENT_CHOICE:
                spdlog::debug("MessageRouter received EquipmentChoice message.");
                equipmentChoiceListener(messageJson.get<spy::network::messages::EquipmentChoice>());
                return;
            case spy::network::messages::MessageTypeEnum::GAME_OPERATION:
                spdlog::debug("MessageRouter received GameOperation message.");
                gameOperationListener(messageJson.get<spy::network::messages::GameOperation>());
                return;
            case spy::network::messages::MessageTypeEnum::GAME_LEAVE:
                spdlog::info("MessageRouter received GameLeave message.");
                gameLeaveListener(messageJson.get<spy::network::messages::GameLeave>());
                return;
            case spy::network::messages::MessageTypeEnum::REQUEST_GAME_PAUSE:
                spdlog::debug("MessageRouter received RequestGamePause message.");
                pauseRequestListener(messageJson.get<spy::network::messages::RequestGamePause>());
                return;
            case spy::network::messages::MessageTypeEnum::REQUEST_META_INFORMATION:
                spdlog::debug("MessageRouter received RequestMetaInformation message.");
                metaInformationRequestListener(messageJson.get<spy::network::messages::RequestMetaInformation>());
                return;
            case spy::network::messages::MessageTypeEnum::REQUEST_REPLAY:
                spdlog::debug("MessageRouter received RequestReplay message.");
                replayRequestListener(messageJson.get<spy::network::messages::RequestReplay>());
                return;
            default:
                spdlog::error("Handling this message type has not been implemented.");
        }
    } catch (nlohmann::json::exception &e) {
        // message doesn't fit to the standard definition --> illegal message error + kick
        spdlog::error("Error parsing JSON from message: {}", e.what());
        spy::network::messages::Error errorMessage{connectionId.value_or(spy::util::UUID{}),
                                                   spy::network::ErrorTypeEnum::ILLEGAL_MESSAGE};
        errorMessage.setDebugMessage("Message doesn't fit to the standardized ones. "
                                     "Exception: " + std::string{e.what()});
        if (connectionId.has_value()) {
            spdlog::error("Sending ILLEGAL_MESSAGE and kicking client");
            sendMessage(errorMessage);
            closeConnection(connectionId.value());
        } else {
            spdlog::error("Sending ILLEGAL_MESSAGE and closing connection");
            sendMessage(connectionPtr, errorMessage);
        }
        return;
    }
}

void
MessageRouter::registerUUIDforConnection(const spy::util::UUID &id, const MessageRouter::connectionPtr &connection) {
    try {
        auto &con = connectionFromPtr(connection);
        con.second = id;
    } catch (const std::invalid_argument &e) {
        spdlog::error("Error registering UUID {}. Exception: {}", id, e.what());
    }
}

MessageRouter::connection &MessageRouter::connectionFromPtr(const MessageRouter::connectionPtr &con) {
    for (auto &c: activeConnections) {
        if (con == c.first) {
            return c;
        }
    }
    throw std::invalid_argument("Connection not in list of known connections");
}

MessageRouter::connection &MessageRouter::connectionFromUUID(const spy::util::UUID &id) {
    for (auto &c: activeConnections) {
        if (id == c.second) {
            return c;
        }
    }
    throw UUIDNotFoundException{};
}

void MessageRouter::clearConnections() {
    activeConnections.clear();
}

void MessageRouter::closeConnection(const spy::util::UUID &id) {
    spdlog::info("MessageRouter: Closing connection to player {}", id);
    try {
        connection con = connectionFromUUID(id);
        activeConnections.erase(std::remove(activeConnections.begin(), activeConnections.end(), con),
                                activeConnections.end());
    } catch (const UUIDNotFoundException &e) {
        spdlog::warn("Connection to {} was already closed! Error: {}", id, e.what());
    }
}

bool MessageRouter::isConnected(const spy::util::UUID &id) const {
    const auto r = std::find_if(activeConnections.begin(), activeConnections.end(),
                                [id](const auto &ptrIdPair) {
                                    return ptrIdPair.second.has_value() and ptrIdPair.second.value() == id;
                                });

    return r != activeConnections.end();
}
