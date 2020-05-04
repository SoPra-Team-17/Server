//
// Created by jonas on 19.04.20.
//
#include <Client/WebSocketClient.hpp>
#include <string>
#include <iostream>
#include <network/messages/Hello.hpp>

using namespace std::string_literals;

int main() {
    auto host = "localhost"s;
    auto path = "/"s;
    auto protocol = "no-time-to-spy"s;

    websocket::network::WebSocketClient c{host, path, 7007, protocol};
    auto hello = spy::network::messages::Hello{spy::util::UUID::generate(),
                                               "Test Client",
                                               spy::network::RoleEnum::PLAYER};
    nlohmann::json hj = hello;
    c.send(hj.dump());
    std::cout << "done" << std::endl;
    std::this_thread::sleep_for(std::chrono::hours{100});
}