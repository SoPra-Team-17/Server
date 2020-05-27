
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
    spdlog::trace("Received message from client {} : {}", con.second.value_or(spy::util::UUID{}), message);
    nlohmann::json messageJson;
    try {
        messageJson = nlohmann::json::parse(message);
    } catch (nlohmann::json::exception &e) {
        spdlog::error("Error parsing JSON from message: {}", e.what());
        return;
    }
    auto messageContainer = messageJson.get<spy::network::MessageContainer>();

    //check if client sends with his own UUID
    if (messageContainer.getType() != spy::network::messages::MessageTypeEnum::HELLO
        && messageContainer.getClientId() != con.second.value()) {
            spdlog::warn("Client {} sent a message with false uuid: {}", con.second.value(), messageContainer.getClientId());
            messageJson.at("clientId") = con.second.value(); // correct the uuid
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
            reconnectListener(messageJson.get<spy::network::messages::Reconnect>());
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
