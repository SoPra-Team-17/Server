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

Server::Server(uint16_t port, unsigned int verbosity, std::string characterPath, std::string matchPath,
               std::string scenarioPath, std::map<std::string, std::string> additionalOptions) :
               port(port), verbosity(verbosity), additionalOptions(std::move(additionalOptions)),
               router(port, "no-time-to-spy") {
    characterPath = "";
    matchPath = "";
    scenarioPath = "";

    std::ifstream ifs{matchPath};
    nlohmann::json j;

    try {
        j = nlohmann::json::parse(ifs);
        matchConfig = j;
    } catch (nlohmann::json::exception &e) {
        std::cerr << "Not a valid json file: " << e.what() << std::endl;
        std::exit(1);
    }
}


void Server::run() {
    spdlog::info("Server is running!");

    // Leave server running forever
//    std::this_thread::sleep_until(
//            std::chrono::system_clock::now() + std::chrono::hours(std::numeric_limits<int>::max()));
}
