/**
 * @file   main.cpp
 * @author Dominik Authaler, Jonas Otto
 * @date   01.04.2020 (creation)
 */

#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include <Server/WebSocketServer.hpp>
#include <datatypes/matchconfig/MatchConfig.hpp>
#include <network/MessageRouter.hpp>

constexpr unsigned int maxVerbosity = 5;
constexpr unsigned int defaultVerbosity = 1;
constexpr unsigned int defaultPort = 7007;

int main(int argc, char *argv[]) {
    CLI::App app;

    std::string characterPath;
    std::string matchPath;
    std::string scenarioPath;

    unsigned int verbosity = defaultVerbosity;
    unsigned int port = defaultPort;

    std::vector<std::string> additionalOptions;

    app.add_option("--config-charset,-c", characterPath,
                   "Path to the character configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--config-match,-m", matchPath,
                   "Path to the match configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--config-scenario,-s", scenarioPath,
                   "Path to the scenario configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--verbosity,-v", verbosity,
                   "Logging verbosity")->check(CLI::Range(maxVerbosity));
    app.add_option("--port,-p", port, "Port used by the server");
    app.add_option("--x", additionalOptions, "Additional key value pairs");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    spdlog::info("Server called with following arguments: ");
    spdlog::info(" -> character configuration: {}", characterPath);
    spdlog::info(" -> match configuration:     {}", matchPath);
    spdlog::info(" -> scenario configuration:  {}", scenarioPath);
    spdlog::info(" -> verbosity:               {}", verbosity);
    spdlog::info(" -> port:                    {}", port);
    spdlog::info(" -> additional:");
    for (unsigned int i = 0; i + 1 < additionalOptions.size(); i += 2) {
        spdlog::info("\t {} = {}", additionalOptions.at(i), additionalOptions.at(i + 1));
    }

    // Example use of LibCommon
    spy::MatchConfig matchConfig;
    std::cout << "Grapple range from matchConfig is " << matchConfig.getGrappleRange() << std::endl;

    MessageRouter router{static_cast<uint16_t>(port), "no-time-to-spy"};

    // Leave server running forever
    std::this_thread::sleep_until(
            std::chrono::system_clock::now() + std::chrono::hours(std::numeric_limits<int>::max()));
}
