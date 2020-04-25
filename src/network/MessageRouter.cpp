#include <spdlog/spdlog.h>

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
