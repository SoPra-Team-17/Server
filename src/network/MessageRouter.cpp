
#include <utility>
#include <algorithm>
#include "MessageRouter.hpp"
#include "spdlog/fmt/ostr.h"

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
    auto &foundConnection = connectionFromPtr(closedConnection);
    spdlog::info("Connection {} closed.", foundConnection.second.value());

    auto con = std::find_if(activeConnections.begin(), activeConnections.end(), [closedConnection](const auto &c) {
        return c.first == closedConnection;
    });
    activeConnections.erase(con);
}

void MessageRouter::receiveListener(const MessageRouter::connectionPtr &connectionPtr, const std::string &message) {
    auto &con = connectionFromPtr(connectionPtr);
    spdlog::info("Received message from client {} : {}", con.second.value_or(spy::util::UUID{}), message);
    nlohmann::json messageJson;
    try {
        messageJson = nlohmann::json::parse(message);
    } catch (nlohmann::json::exception &e) {
        spdlog::error("Error parsing JSON from message: {}", e.what());
        return;
    }
    auto messageContainer = messageJson.get<spy::network::MessageContainer>();
    switch (messageContainer.getType()) {
        // TODO: missing cases
        case spy::network::messages::MessageTypeEnum::INVALID:
            spdlog::error("Received message with invalid type: " + message);
            return;
        case spy::network::messages::MessageTypeEnum::HELLO:
            spdlog::info("MessageRouter received Hello message.");
            helloListener(messageJson.get<spy::network::messages::Hello>(), connectionPtr);
            return;
        case spy::network::messages::MessageTypeEnum::HELLO_REPLY:
            break;
        case spy::network::messages::MessageTypeEnum::RECONNECT:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_STARTED:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_ITEM_CHOICE:
            break;
        case spy::network::messages::MessageTypeEnum::ITEM_CHOICE:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_EQUIPMENT_CHOICE:
            break;
        case spy::network::messages::MessageTypeEnum::EQUIPMENT_CHOICE:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_STATUS:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_GAME_OPERATION:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_OPERATION:
            break;
        case spy::network::messages::MessageTypeEnum::STATISTICS:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_LEAVE:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_LEFT:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_GAME_PAUSE:
            break;
        case spy::network::messages::MessageTypeEnum::GAME_PAUSE:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_META_INFORMATION:
            break;
        case spy::network::messages::MessageTypeEnum::META_INFORMATION:
            break;
        case spy::network::messages::MessageTypeEnum::STRIKE:
            break;
        case spy::network::messages::MessageTypeEnum::ERROR:
            break;
        case spy::network::messages::MessageTypeEnum::REQUEST_REPLAY:
            break;
        case spy::network::messages::MessageTypeEnum::REPLAY:
            break;
    }
    spdlog::error("Handling this message type has not been implemented yet.");
}

void
MessageRouter::registerUUIDforConnection(const spy::util::UUID &id, const MessageRouter::connectionPtr &connection) {
    auto &con = connectionFromPtr(connection);
    con.second = id;
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
    throw std::invalid_argument("UUID not in list of known connections");
}
