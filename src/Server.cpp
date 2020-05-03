/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   02.05.2020 (creation)
 * @brief  Implementation of the server class.
 */

#include "Server.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <iostream>
#include <utility>

Server::Server(uint16_t port, unsigned int verbosity, const std::string &characterPath, const std::string &matchPath,
               const std::string &scenarioPath, std::map<std::string, std::string> additionalOptions) :
               port(port), verbosity(verbosity), additionalOptions(std::move(additionalOptions)),
               router(port, "no-time-to-spy") {

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
        characterDescriptions = j.get<std::vector<spy::character::CharacterDescription>>();
        spdlog::info("Successfully read character descriptions");
        ifs.close();
    } catch (nlohmann::json::exception &e) {
        spdlog::error("JSON file is invalid: " + std::string(e.what()));
        ifs.close();
        std::exit(1);
    }
}
