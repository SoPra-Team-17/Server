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
        std::cout << ifs.rdbuf() << std::endl;
        j = nlohmann::json::parse(ifs);
        matchConfig = j.get<spy::MatchConfig>();

        ifs.open(scenarioPath);
        std::cout << ifs.rdbuf() << std::endl;
        j = nlohmann::json::parse(ifs);
        scenarioConfig = j.get<spy::scenario::Scenario>();

        ifs.open(characterPath);
        j = nlohmann::json::parse(ifs);
        characterDescriptions = j.get<std::vector<spy::character::CharacterDescription>>();
        ifs.close();
    } catch (nlohmann::json::exception &e) {
        spdlog::error("JSON file is invalid: " + std::string(e.what()));
        std::exit(1);
    }
}


void Server::run() {
    spdlog::info("Server is running!");

    // Leave server running forever
//    std::this_thread::sleep_until(
//            std::chrono::system_clock::now() + std::chrono::hours(std::numeric_limits<int>::max()));
}
