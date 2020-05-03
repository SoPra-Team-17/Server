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

const std::map<unsigned int, spdlog::level::level_enum> Server::verbosityMap = {
        {0, spdlog::level::level_enum::trace},
        {1, spdlog::level::level_enum::err},
        {2, spdlog::level::level_enum::critical},
        {3, spdlog::level::level_enum::warn},
        {4, spdlog::level::level_enum::debug},
        {5, spdlog::level::level_enum::trace}
};

Server::Server(uint16_t port, unsigned int verbosity, const std::string &characterPath, const std::string &matchPath,
               const std::string &scenarioPath, std::map<std::string, std::string> additionalOptions) :
               verbosity(verbosity), additionalOptions(std::move(additionalOptions)),
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
        characterDescriptions = j.get<std::vector<spy::character::CharacterDescription>>();
        spdlog::info("Successfully read character descriptions");
        ifs.close();
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("JSON file is invalid: " + std::string(e.what()));
        ifs.close();
        std::exit(1);
    }
}

void Server::configureLogging() const {
    std::vector<spdlog::sink_ptr> sinks;
    std::string logFile(30, '\0');

    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::strftime(&logFile[0], logFile.size(), "%m-%d_%H:%M:%S.txt", std::localtime(&now));

    auto it = verbosityMap.find(verbosity);
    if (it == verbosityMap.end()) {
        spdlog::error("Requested verbosity level {} is not supported!", verbosity);
        std::exit(1);
    }

    // logging to console can be influenced via verbosity setting
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(it->second);

    // logging to file always works with max logging level
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/" + logFile);
    fileSink->set_level(spdlog::level::level_enum::trace);

    sinks.push_back(consoleSink);
    sinks.push_back(fileSink);

    auto combined_logger = std::make_shared<spdlog::logger>("Logger", begin(sinks), end(sinks));

    spdlog::flush_every(std::chrono::seconds(5));

    // use this new combined sink as default logger
    spdlog::set_default_logger(combined_logger);
}