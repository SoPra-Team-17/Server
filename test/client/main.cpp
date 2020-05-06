//
// Created by jonas on 19.04.20.
//
#include <Client/WebSocketClient.hpp>
#include <string>
#include <iostream>
#include <network/messages/Hello.hpp>

using namespace std::string_literals;

struct TestClient {
    spy::util::UUID id;
    websocket::network::WebSocketClient wsClient{"localhost", "/", 7007, "no-time-to-spy"};
    std::string name;

    explicit TestClient(const std::string &clientName) {
        name = clientName;
        wsClient.receiveListener.subscribe([this](const std::string &message) {
            std::cout << name << ": " << message << std::endl;
        });
    }

    void sendHello() {
        auto hello = spy::network::messages::Hello{id, name, spy::network::RoleEnum::PLAYER};
        nlohmann::json hj = hello;
        wsClient.send(hj.dump());
    }
};

int main() {
    auto c1 = TestClient("TestClient1");
    auto c2 = TestClient("TestClient2");

    c1.sendHello();
    std::this_thread::sleep_for(std::chrono::seconds{5});
    c2.sendHello();

    std::cout << "done" << std::endl;
    std::this_thread::sleep_for(std::chrono::hours{100});
}