/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   02.05.2020 (creation)
 * @brief  Implementation of the server class.
 */

#include "Server.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <utility>
#include <datatypes/character/CharacterInformation.hpp>
#include <network/messages/HelloReply.hpp>

const std::map<unsigned int, spdlog::level::level_enum> Server::verbosityMap = {
        {0, spdlog::level::level_enum::trace},
        {1, spdlog::level::level_enum::off},
        {2, spdlog::level::level_enum::critical},
        {3, spdlog::level::level_enum::err},
        {4, spdlog::level::level_enum::warn},
        {5, spdlog::level::level_enum::info},
        {6, spdlog::level::level_enum::debug},
        {7, spdlog::level::level_enum::trace}
};

Server::Server(uint16_t port, unsigned int verbosity, const std::string &characterPath, const std::string &matchPath,
               const std::string &scenarioPath, std::map<std::string, std::string> additionalOptions) :
        verbosity(verbosity),
        additionalOptions(std::move(additionalOptions)),
        router(port, "no-time-to-spy") {
    configureLogging();

    spdlog::info("Server called with following arguments: ");
    spdlog::info(" -> character configuration: {}", characterPath);
    spdlog::info(" -> match configuration:     {}", matchPath);
    spdlog::info(" -> scenario configuration:  {}", scenarioPath);
    spdlog::info(" -> verbosity:               {}", verbosity);
    spdlog::info(" -> port:                    {}", port);
    if (!this->additionalOptions.empty()) {
        spdlog::info(" -> additional:");
        for (const auto &elem : this->additionalOptions) {
            spdlog::info("\t {} = {}", elem.first, elem.second);
        }
    }

    // load configuration files
    loadConfigs(matchPath, scenarioPath, characterPath);

    if (characterInformations.size() < 10) {
        spdlog::critical("Not enough character descriptions given, at least 10 are needed for choice phase!");
        std::exit(1);
    }

    spdlog::info("Cat UUID is {}", catId);
    spdlog::info("Janitor UUID is {}", janitorId);
    
    gameState = spy::gameplay::State{0, spy::scenario::FieldMap{scenarioConfig}, {}, {}, std::nullopt,
                                     std::nullopt};

    // check if the scenario contains enough fields needed to place all characters + cat + janitor
    unsigned int accessibleFields = 0;
    gameState.getMap().forAllFields([&accessibleFields](const spy::scenario::Field &f) {
        if (f.getFieldState() == spy::scenario::FieldStateEnum::FREE
            || f.getFieldState() == spy::scenario::FieldStateEnum::BAR_SEAT) {
            accessibleFields++;
        }
    });

    // Explanation of the number 10: 2x max. 4 characters + cat + janitor
    if (accessibleFields < maxNumberOfNPCs + 10) {
        spdlog::critical("Not enough accessible fields to place all characters with cat and janitor, at least"
                         "{} are needed for the selected amount of NPCs", maxNumberOfNPCs + 10);
        std::exit(1);
    }

    using serverFSM = afsm::state_machine<Server>;
    auto &fsm = static_cast<serverFSM &>(*this);

    router.addHelloListener([&fsm, this](spy::network::messages::Hello msg, MessageRouter::connectionPtr con) {
        // New clients send Hello, and need to be assigned a UUID immediately.
        // This new UUID gets inserted into the HelloMessage, so the FSM receives properly formatted HelloMessage
        spdlog::info("Server received Hello message, initializing UUID");
        msg.setClientId(spy::util::UUID::generate());
        spdlog::info("Registering UUID {} at router", msg.getClientId());
        router.registerUUIDforConnection(msg.getClientId(), con);
        spdlog::info("Posting event to FSM now");
        clientRoles[msg.getClientId()] = msg.getRole();
        fsm.process_event(msg);
    });

    auto forwardMessage = [&fsm, this](auto msg) {
        auto clientRole = clientRoles.at(msg.getClientId());

        if (Util::isAllowedMessage(clientRole, msg)) {
            fsm.process_event(msg);
        } else {
            // message dropped
            if (clientRole == spy::network::RoleEnum::AI) {
                spdlog::critical("Client {} with role AI was kicked due to role filtering for {} message",
                        msg.getClientId(), fmt::json(msg.getType()));
                fsm.process_event(events::kickAI{msg.getClientId()});
            } else {
                spdlog::warn("Client {} sent an {} message that was dropped due to role filtering",
                             msg.getClientId(), fmt::json(msg.getType()));
            }
        }
    };

    auto discardNotImplemented = [](auto msg) {
        spdlog::warn("Received message of type {}, handling is not implemented.", fmt::json(msg.getType()));
    };

    router.addItemChoiceListener(forwardMessage);
    router.addEquipmentChoiceListener(forwardMessage);
    router.addGameOperationListener(forwardMessage);
    router.addPauseRequestListener(forwardMessage);
    router.addMetaInformationRequestListener(forwardMessage);
    router.addReplayRequestListener(discardNotImplemented);
    router.addGameLeaveListener(forwardMessage);

    router.addReconnectListener(
            [this, forwardMessage](const spy::network::messages::Reconnect &msg,
                                   const MessageRouter::connectionPtr &con) {
                if (msg.getSessionId() != sessionId) {
                    spdlog::warn(
                            "Reconnect message from client {} specifies sessionId {}, but current sessionId is {}.",
                            msg.getClientId(),
                            msg.getSessionId(),
                            sessionId);
                    spy::network::messages::Error errorMessage{msg.getClientId(),
                                                               spy::network::ErrorTypeEnum::SESSION_DOES_NOT_EXIST};
                    spdlog::warn("Sending SESSION_DOES_NOT_EXIST error message");
                    router.registerUUIDforConnection(msg.getClientId(), con);
                    router.sendMessage(errorMessage);
                    router.closeConnection(msg.getClientId());
                    return;
                }

                // Check if reconnect is from disconnected player
                const spy::util::UUID &clientId = msg.getClientId();
                if (!Util::isDisconnectedPlayer(clientId, playerIds, router)) {
                    spdlog::warn("Received reconnect from client {}, which is not currently disconnected.", clientId);
                    return;
                }

                spdlog::info("Server received Reconnect message, with client ID {}", msg.getClientId());
                spdlog::info("Registering client UUID {} at router after reconnect", msg.getClientId());
                router.registerUUIDforConnection(msg.getClientId(), con);
                forwardMessage(msg);
            });

    router.addDisconnectListener([&fsm, this](const spy::util::UUID &uuid) {
        auto clientRole = clientRoles.at(uuid);
        using spy::network::RoleEnum;
        if (clientRole == RoleEnum::PLAYER or clientRole == RoleEnum::AI) {
            fsm.process_event(events::playerDisconnect{uuid});
        } else {
            spdlog::info("Client {} (Role: {}) disconnected.", uuid, fmt::json(clientRole));
        }
    });
}

void Server::configureLogging() const {
    std::vector<spdlog::sink_ptr> sinks;
    std::string logFile(30, '\0');
    struct tm buf = {};

    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::strftime(&logFile[0], logFile.size(), "%m-%d_%H:%M:%S.txt", localtime_r(&now, &buf));

    auto it = verbosityMap.find(verbosity);
    if (it == verbosityMap.end()) {
        spdlog::error("Requested verbosity level {} is not supported!", verbosity);
        std::exit(1);
    }

    // logging to console can be influenced via verbosity setting
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_color_mode(spdlog::color_mode::always);
    consoleSink->set_level(it->second);

    // logging to file always works with max logging level
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/" + logFile);
    fileSink->set_level(spdlog::level::level_enum::trace);

    sinks.push_back(consoleSink);
    sinks.push_back(fileSink);

    auto combined_logger = std::make_shared<spdlog::logger>("Logger", begin(sinks), end(sinks));

    // Flush for every message of level info or higher
    combined_logger->flush_on(spdlog::level::info);

    combined_logger->set_level(spdlog::level::trace);

    // use this new combined sink as default logger
    spdlog::set_default_logger(combined_logger);
}

void Server::loadConfigs(const std::string &matchPath,
                         const std::string &scenarioPath,
                         const std::string &characterPath) {
    std::ifstream ifs{matchPath};
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(ifs);
        matchConfig = j.get<spy::MatchConfig>();
        spdlog::info("Successfully read match configuration");

        ifs = std::ifstream(scenarioPath);
        j = nlohmann::json::parse(ifs);
        scenarioConfig = j.get<spy::scenario::Scenario>();
        spdlog::info("Successfully read scenario configuration");

        ifs = std::ifstream(characterPath);
        j = nlohmann::json::parse(ifs);

        for (const auto &characterDescriptionJson: j) {
            // Read CharacterDescription, save CharacterInformation = CharacterDescription + UUID
            auto characterDescription = characterDescriptionJson.get<spy::character::CharacterDescription>();
            auto uuid = spy::util::UUID::generate();
            spdlog::info("Character {} has UUID {}", characterDescription.getName(), uuid);
            characterInformations.emplace_back(std::move(characterDescription), uuid);
        }

        spdlog::info("Successfully read character descriptions");
        ifs.close();
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("JSON file is invalid: " + std::string(e.what()));
        ifs.close();
        std::exit(1);
    }
}
