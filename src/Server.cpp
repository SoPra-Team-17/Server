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
#include <network/MessageTypeTraits.hpp>

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

    std::ifstream ifs{matchPath};
    nlohmann::json j;

    // load configuration files
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
            characterInformations.emplace_back(std::move(characterDescription), spy::util::UUID::generate());
        }

        spdlog::info("Successfully read character descriptions");
        ifs.close();
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("JSON file is invalid: " + std::string(e.what()));
        ifs.close();
        std::exit(1);
    }

    if (characterInformations.size() < 10) {
        spdlog::critical("No enough character descriptions given, at least 10 are needed for choice phase!");
        std::exit(1);
    }

    gameState = spy::gameplay::State{1, spy::scenario::FieldMap{scenarioConfig}, {}, {}, spy::util::Point{1, 1},
                                     spy::util::Point{}};

    using serverFSM = afsm::state_machine<Server>;
    auto &fsm = static_cast<serverFSM &>(*this);

    router.addHelloListener([&fsm, this](spy::network::messages::Hello msg, MessageRouter::connectionPtr con) {
        // New clients send Hello, and need to be assigned a UUID immediately.
        // This new UUID gets inserted into the HelloMessage, so the FSM receives properly formatted HelloMessage
        spdlog::info("Server received Hello message, initializing UUID");
        msg = spy::network::messages::Hello{spy::util::UUID::generate(), msg.getName(), msg.getRole()};
        spdlog::info("Registering UUID {} at router", msg.getClientId());
        router.registerUUIDforConnection(msg.getClientId(), con);
        spdlog::info("Posting event to FSM now");
        clientRoles[msg.getClientId()] = msg.getRole();
        fsm.process_event(msg);
    });

    router.addGameLeaveListener([&fsm, this](spy::network::messages::GameLeave msg) {
        clientRoles.erase(msg.getClientId());
        fsm.process_event(msg);
    });

    auto forwardMessage = [&fsm, this](auto msg) {
        auto clientRole = clientRoles.at(msg.getClientId());

        if (clientRole == spy::network::RoleEnum::PLAYER
            && receivableFromPlayer<decltype(msg)>::value) {
            fsm.process_event(msg);
        } else if (clientRole == spy::network::RoleEnum::AI
                   && receivableFromAI<decltype(msg)>::value) {
            fsm.process_event(msg);
        } else if (clientRole == spy::network::RoleEnum::SPECTATOR
                   && receivableFromSpectator<decltype(msg)>::value) {
            fsm.process_event(msg);
        } else {
            // message dropped
            spdlog::warn("Client {} sent an {} message that was dropped due to role filtering",
                    msg.getClientId(), fmt::json(msg.getType()));
        }
    };

    auto discardNotImplemented = [](auto msg){
        spdlog::warn("Received message of type {}, handling is not implemented.", fmt::json(msg.getType()));
    };

    router.addReconnectListener(discardNotImplemented);
    router.addItemChoiceListener(forwardMessage);
    router.addEquipmentChoiceListener(forwardMessage);
    router.addGameOperationListener(forwardMessage);
    router.addPauseRequestListener(discardNotImplemented);
    router.addMetaInformationRequestListener(forwardMessage);
    router.addReplayRequestListener(discardNotImplemented);
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
