//
// Created by jonas on 19.04.20.
//
#include <Client/WebSocketClient.hpp>
#include <string>
#include <iostream>

using namespace std::string_literals;

int main(int argc, char *argv[]) {
    auto host = "localhost"s;
    auto path = "/"s;
    auto protocol = "no-time-to-spy"s;

    websocket::network::WebSocketClient c{host, path, 7007, protocol};
    c.send("Hi!");
    std::cout << "done" << std::endl;
    std::this_thread::sleep_for(std::chrono::hours{100});

}