/**
 * @file   main.cpp
 * @author Dominik Authaler
 * @date   30.03.2020 (creation)
 * @brief  Small example to test the functionality of CLI11.
 */

#include "CLI/CLI.hpp"

constexpr int maxVerbosity = 5;

int main(int argc, char *argv[]) {
    CLI::App app;
    std::string characterPath;
    std::string matchPath;
    std::string scenarioPath;
    unsigned int verbosity = 1;
    unsigned int port = 17;
    std::vector<std::string> additionalOptions;

    app.add_option("--config-charset,-c", characterPath, "Path to the character configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--config-match,-m", matchPath, "Path to the match configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--config-scenario,-s", scenarioPath, "Path to the scenario configuration file")->required()->check(CLI::ExistingFile);
    app.add_option("--verbosity,-v", verbosity, "Logging verbosity")->check(CLI::Range(0, maxVerbosity));
    app.add_option("--port,-p", port, "Port used by the server");
    app.add_option("--x", additionalOptions, "Additional key value pairs");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Character configuration: " << characterPath << std::endl;
    std::cout << "Match configuration:     " << matchPath     << std::endl;
    std::cout << "Scenario configuration:  " << scenarioPath  << std::endl;
    std::cout << "Verbosity:               " << verbosity     << std::endl;
    std::cout << "Port:                    " << port          << std::endl;
    std::cout << "Additional options: " << std::endl;
    for (unsigned int i = 0; i < additionalOptions.size(); i+= 2) {
        std::cout << "Key: " << additionalOptions[i] << "\tValue: " << additionalOptions[i+1] << std::endl;
    }

    return 0;
}


