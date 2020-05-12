//
// Created by jonas on 19.04.20.
//
#include <Client/WebSocketClient.hpp>
#include <string>
#include <iostream>
#include <network/messages/Hello.hpp>
#include <variant>
#include <random>
#include <network/messages/ItemChoice.hpp>
#include <network/messages/RequestItemChoice.hpp>
#include <network/messages/HelloReply.hpp>
#include "datatypes/gadgets/GadgetEnum.hpp"
#include <spdlog/spdlog.h>
#include <network/messages/RequestEquipmentChoice.hpp>
#include <network/messages/EquipmentChoice.hpp>
#include <network/messages/RequestGamePause.hpp>
#include <network/messages/GameLeave.hpp>

using namespace std::string_literals;

struct TestClient {
    spy::util::UUID id;
    websocket::network::WebSocketClient wsClient{"localhost", "/", 7007, "no-time-to-spy"};
    std::string name;
    spy::network::RoleEnum role;
    unsigned int choiceCounter = 0;
    unsigned int numberOfCharacters = 0;
    std::random_device rd{};
    std::mt19937 rng{rd()};

    explicit TestClient(std::string clientName, spy::network::RoleEnum clientRole = spy::network::RoleEnum::PLAYER) :
            name(std::move(clientName)),
            role(clientRole) {
        using spy::network::messages::MessageTypeEnum;

        spdlog::set_level(spdlog::level::level_enum::trace);

        // decide randomly how many characters are chosen [2 - 4]
        std::uniform_int_distribution<unsigned int> randNum(2, 4);
        numberOfCharacters = randNum(rng);

        spdlog::trace("{} will choose {} characters and {} gadgets", name, numberOfCharacters, 8 - numberOfCharacters);

        wsClient.closeListener.subscribe([clientName]() {
            spdlog::critical("{}: Connection Closed", clientName);
        });

        wsClient.receiveListener.subscribe([this](const std::string &message) {
            spdlog::trace("{}: {}", name, message);

            auto j = nlohmann::json::parse(message);
            auto container = j.get<spy::network::MessageContainer>();

            switch (container.getType()) {
                case MessageTypeEnum::REQUEST_ITEM_CHOICE: {
                    std::variant<spy::util::UUID, spy::gadget::GadgetEnum> choice;
                    auto offer = j.get<spy::network::messages::RequestItemChoice>();

                    spdlog::info("{} received Request item choice nr. {}", name, choiceCounter);
                    std::uniform_int_distribution<unsigned int> randNum(0, 2);
                    auto chosenIdx = randNum(rng);

                    if (choiceCounter < numberOfCharacters) {
                        choice = offer.getOfferedCharacterIds()[chosenIdx];
                    } else if (choiceCounter < 8) {
                        choice = offer.getOfferedGadgets()[chosenIdx];
                    } else {
                        spdlog::critical("### ERROR, server offering to many rounds! ###");
                        std::exit(1);
                    }

                    sendChoice(choice);
                    choiceCounter++;
                    break;
                }

                case MessageTypeEnum::REQUEST_EQUIPMENT_CHOICE: {
                    auto m = j.get<spy::network::messages::RequestEquipmentChoice>();

                    spdlog::info("{} received Request equipment choice", name);

                    std::set<spy::gadget::GadgetEnum> gadgetSet;
                    std::map<spy::util::UUID, std::set<spy::gadget::GadgetEnum>> choice;

                    // Add all characters with empty gadget set to the map
                    for (const auto &c : m.getChosenCharacterIds()) {
                        choice[c] = std::set<spy::gadget::GadgetEnum>();
                    }

                    // decide randomly to which character the gadget is added
                    std::uniform_int_distribution<unsigned int> randNum(0, numberOfCharacters - 1);
                    for (const auto g : m.getChosenGadgets()) {
                        auto charNum = randNum(rng);
                        choice.at(m.getChosenCharacterIds().at(charNum)).insert(g);
                    }

                    sendEquipmentChoice(choice);
                    choiceCounter++;
                    break;
                }

                case MessageTypeEnum::HELLO_REPLY: {
                    auto m = j.get<spy::network::messages::HelloReply>();
                    id = m.getClientId();

                    spdlog::info("{} was assigned id: {}", name, id.to_string());
                    break;
                }

                default:
                    spdlog::warn("{} received unhandled message : {}", name, message);
                    break;
            }
        });
    }

    void sendChoice(std::variant<spy::util::UUID, spy::gadget::GadgetEnum> choice) {
        spy::network::messages::ItemChoice message(id, choice);
        nlohmann::json mj = message;
        wsClient.send(mj.dump());

        spdlog::info("{} sent item choice\n", name);
    }

    void sendEquipmentChoice(std::map<spy::util::UUID, std::set<spy::gadget::GadgetEnum>> choice) {
        spy::network::messages::EquipmentChoice message(id, choice);
        nlohmann::json mj = message;
        wsClient.send(mj.dump());

        spdlog::info("{} sent equipment choice\n", name);
    }

    void sendHello() {
        auto hello = spy::network::messages::Hello{id, name, role};
        nlohmann::json hj = hello;
        wsClient.send(hj.dump());
    }

    void sendRequestPause() {
        auto msg = spy::network::messages::RequestGamePause{id, true};
        nlohmann::json mj = msg;
        wsClient.send(mj.dump());
    }

    void sendGameLeave() {
        auto msg = spy::network::messages::GameLeave{id};
        nlohmann::json mj = msg;
        wsClient.send(mj.dump());
    }
};

int main() {
    auto c1 = TestClient("TestClient1");
    auto c2 = TestClient("TestClient2", spy::network::RoleEnum::AI);

    c1.sendHello();
    std::this_thread::sleep_for(std::chrono::seconds{1});
    c2.sendHello();

    std::cout << "done" << std::endl;
    std::this_thread::sleep_for(std::chrono::hours{100});
}