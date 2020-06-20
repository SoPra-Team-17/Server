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
#include <network/messages/RequestMetaInformation.hpp>
#include <network/messages/MetaInformation.hpp>
#include <network/messages/Reconnect.hpp>
#include <util/Format.hpp>

using namespace std::string_literals;

struct TestClient {
    spy::util::UUID id;
    std::optional<websocket::network::WebSocketClient> wsClient;
    std::string name;
    spy::network::RoleEnum role;
    spy::util::UUID sessionId;
    unsigned int choiceCounter = 0;
    unsigned int numberOfCharacters = 0;
    std::random_device rd{};
    std::mt19937 rng{rd()};

    explicit TestClient(std::string clientName, spy::network::RoleEnum clientRole = spy::network::RoleEnum::PLAYER) :
            name(std::move(clientName)),
            role(clientRole) {
        spdlog::set_level(spdlog::level::level_enum::trace);

        connect();

        if (role == spy::network::RoleEnum::PLAYER || role == spy::network::RoleEnum::AI) {
            // decide randomly how many characters are chosen [2 - 4]
            std::uniform_int_distribution<unsigned int> randNum(2, 4);
            numberOfCharacters = randNum(rng);

            spdlog::trace("{} will choose {} characters and {} gadgets", name, numberOfCharacters,
                          8 - numberOfCharacters);
        }
    }

    void sendChoice(std::variant<spy::util::UUID, spy::gadget::GadgetEnum> choice) {
        spy::network::messages::ItemChoice message(id, choice);
        nlohmann::json mj = message;
        wsClient.value().send(mj.dump());

        spdlog::info("{} sent item choice\n", name);
    }

    void sendEquipmentChoice(std::map<spy::util::UUID, std::set<spy::gadget::GadgetEnum>> choice) {
        spy::network::messages::EquipmentChoice message(id, choice);
        nlohmann::json mj = message;
        wsClient.value().send(mj.dump());

        spdlog::info("{} sent equipment choice\n", name);
    }

    void sendHello() {
        auto hello = spy::network::messages::Hello{id, name, role};
        nlohmann::json hj = hello;
        wsClient.value().send(hj.dump());
    }

    void sendRequestPause(bool pause) {
        auto msg = spy::network::messages::RequestGamePause{id, pause};
        nlohmann::json mj = msg;
        wsClient.value().send(mj.dump());
    }

    void sendGameLeave() {
        auto msg = spy::network::messages::GameLeave{id};
        nlohmann::json mj = msg;
        wsClient.value().send(mj.dump());
    }

    void requestMetaInformation() {
        spdlog::info("{}: requesting meta information", name);

        using namespace spy::network::messages;
        auto msg = RequestMetaInformation{id, {MetaInformationKey::CONFIGURATION_SCENARIO,
                                               MetaInformationKey::SPECTATOR_COUNT,
                // Expected: only own characters get returned
                                               MetaInformationKey::FACTION_PLAYER1,
                                               MetaInformationKey::FACTION_PLAYER2,
                                               MetaInformationKey::FACTION_NEUTRAL,
                                               MetaInformationKey::GADGETS_PLAYER1,
                                               MetaInformationKey::GADGETS_PLAYER2}};
        nlohmann::json mj = msg;
        wsClient.value().send(mj.dump());
    }

    void connect() {
        using spy::network::messages::MessageTypeEnum;

        wsClient.emplace("localhost", "/", 7007, "no-time-to-spy");

        wsClient.value().closeListener.subscribe([clientName = name]() {
            spdlog::critical("{}: Connection Closed", clientName);
        });

        wsClient.value().receiveListener.subscribe([this](const std::string &message) {
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
                    sessionId = m.getSessionId();

                    spdlog::info("{} was assigned id: {}, sessionId is {}",
                                 name,
                                 id.to_string(),
                                 sessionId.to_string());
                    break;
                }

                case MessageTypeEnum::META_INFORMATION: {
                    auto m = j.get<spy::network::messages::MetaInformation>();
                    auto map = m.getInformation();

                    std::string keys;

                    for (const auto &[key, value] : map) {
                        keys += fmt::json(key);
                    }

                    spdlog::info("{} received keys {}", name, keys);
                    break;
                }


                default:
                    spdlog::warn("{} received unhandled message : {}", name, message);
                    break;
            }
        });
    }

    void disconnect() {
        wsClient.reset();
    }

    void reconnect(bool wrongSessionId = false) {
        connect();
        nlohmann::json m;

        if (wrongSessionId) {
            spy::network::messages::Reconnect message{id, spy::util::UUID::generate()};
            m = message;
        } else {
            spy::network::messages::Reconnect message{id, sessionId};
            m = message;
        }

        wsClient.value().send(m.dump());
    }
};

int main() {
    auto p1 = TestClient("Player 1");
    auto p2 = TestClient("Player 2", spy::network::RoleEnum::AI);
    auto s1 = TestClient("Spectator1", spy::network::RoleEnum::SPECTATOR);
    auto s2 = TestClient("Spectator2", spy::network::RoleEnum::SPECTATOR);
    auto s3 = TestClient("Spectator3", spy::network::RoleEnum::SPECTATOR);

    p1.sendHello();
    s1.sendHello();                 // hello before game start --> no game status expected
    std::this_thread::sleep_for(std::chrono::seconds{1});
    p2.sendHello();
    std::this_thread::sleep_for(std::chrono::milliseconds {100});
    s2.sendHello();                 // hello during choice phases --> no game status expected

    std::this_thread::sleep_for(std::chrono::seconds{5});

    p1.requestMetaInformation();
    p2.requestMetaInformation();
    s1.requestMetaInformation();

    // Pause, unpause within time limit
    p1.sendRequestPause(true);
    std::this_thread::sleep_for(std::chrono::seconds{3});
    // Unpause
    p1.sendRequestPause(false);
    std::this_thread::sleep_for(std::chrono::seconds{3});

    // Pause with disconnect
    p1.sendRequestPause(true);
    std::this_thread::sleep_for(std::chrono::seconds{2});
    p1.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{3});
    p2.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{3});
    p1.reconnect();
    std::this_thread::sleep_for(std::chrono::seconds{3});
    p2.reconnect();

    // Both player disconnect + reconnect within time limit
    p2.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{3});
    p1.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{10});
    p1.reconnect();
    p2.reconnect();
    std::this_thread::sleep_for(std::chrono::seconds{3});

    // Disconnect and reconnect with false session Id (error message expected)
    p1.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{1});
    p1.reconnect(true);

    // Disconnect without reconnect (timeout and game end expected)
    p1.disconnect();
    std::this_thread::sleep_for(std::chrono::seconds{21});



    std::cout << "done" << std::endl;
}
