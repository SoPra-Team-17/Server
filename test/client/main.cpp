//
// Created by jonas on 19.04.20.
//
#include <Client/WebSocketClient.hpp>
#include <string>
#include <iostream>
#include <network/messages/Hello.hpp>
#include <variant>
#include <network/messages/ItemChoice.hpp>
#include <network/messages/RequestItemChoice.hpp>
#include <network/messages/HelloReply.hpp>
#include "datatypes/gadgets/GadgetEnum.hpp"
#include <spdlog/spdlog.h>

using namespace std::string_literals;

struct TestClient {
    spy::util::UUID id;
    websocket::network::WebSocketClient wsClient{"localhost", "/", 7007, "no-time-to-spy"};
    std::string name;
    unsigned int choiceCounter = 0;

    explicit TestClient(const std::string &clientName) {
        using spy::network::messages::MessageTypeEnum;

        name = clientName;
        wsClient.receiveListener.subscribe([this](const std::string &message) {
            std::cout << name << ": " << message << std::endl;

            auto j = nlohmann::json::parse(message);
            auto container = j.get<spy::network::MessageContainer>();

            switch(container.getType()) {
                case MessageTypeEnum::REQUEST_ITEM_CHOICE: {
                    std::variant<spy::util::UUID, spy::gadget::GadgetEnum> choice;
                    auto offer = j.get<spy::network::messages::RequestItemChoice>();

                    spdlog::info("{} received Request item choice", name);

                    if (choiceCounter < 3) {
                        // select character in the first 3 offers
                        choice = offer.getOfferedCharacterIds()[0];
                    } else if (choiceCounter < 8) {
                        // select character in the second 3 offers
                        choice = offer.getOfferedGadgets()[0];
                    } else {
                        std::cout << "### ERROR, server offering to many rounds! ###" << std::endl;
                    }

                    sendChoice(choice);
                    choiceCounter++;
                    break;
                }

                case MessageTypeEnum::HELLO_REPLY: {
                    auto m = j.get<spy::network::messages::HelloReply>();
                    id = m.getclientId();

                    std::cout << name << ": was assigned id: " << id << std::endl;
                    break;
                }

                default:
                    break;
            }
        });
    }

    void sendChoice(std::variant<spy::util::UUID, spy::gadget::GadgetEnum> choice) {
        spy::network::messages::ItemChoice message(id, choice);
        nlohmann::json hj = message;
        wsClient.send(hj.dump());

        spdlog::info("{} sent item choice", name);
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