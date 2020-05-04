
#include <utility>
#include "MessageRouter.hpp"

MessageRouter::MessageRouter(uint16_t port, std::string protocol) : server{port, std::move(protocol)} {
    server.connectionListener.subscribe(
            [this](std::shared_ptr<websocket::network::Connection> newConnection) {
                connectListener(std::move(newConnection));
            });
    server.closeListener.subscribe(
            [this](std::shared_ptr<websocket::network::Connection> closedConnection) {
                disconnectListener(std::move(closedConnection));
            });
}

void MessageRouter::connectListener(std::shared_ptr<websocket::network::Connection> newConnection) {
    // Assign lowest unused index
    int index = 0;
    for (const auto &conn : activeConnections) {
        if (conn.first == index) {
            index++;
        }
    }
    spdlog::info("New connection (Nr {})", index);
    activeConnections.emplace(index, newConnection);
    newConnection->receiveListener.subscribe(
            std::bind(&MessageRouter::receiveListener, this, index, newConnection, std::placeholders::_1));
}

void MessageRouter::disconnectListener(std::shared_ptr<websocket::network::Connection> closedConnection) {
    auto foundConnection = std::find_if(activeConnections.begin(), activeConnections.end(), [&](const auto &mapEntry) {
        return mapEntry.second == closedConnection;
    });
    if (foundConnection == activeConnections.end()) {
        spdlog::error("MessageRouter::disconnectListener called for unknown connection.");
        throw std::invalid_argument("connection not in map of known client connections");
    }
    spdlog::info("Connection {} closed.", foundConnection->first);
    activeConnections.erase(foundConnection->first);
}

void MessageRouter::receiveListener(int connectionIndex,
                                    std::shared_ptr<websocket::network::Connection> /*connection*/,
                                    std::string message) {
    spdlog::info("Received message from client {} : {}", connectionIndex, message);
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
            spdlog::info("MessageRouter received Hello message. Calling listeners now.");
            helloListener(messageJson.get<spy::network::messages::Hello>());
            break;
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
