/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   02.05.2020 (creation)
 * @brief  Declaration of the server class.
 */

#ifndef SERVER017_SERVER_HPP
#define SERVER017_SERVER_HPP

#include <network/MessageRouter.hpp>
#include "datatypes/matchconfig/MatchConfig.hpp"
#include "datatypes/scenario/Scenario.hpp"
#include "datatypes/character/CharacterDescription.hpp"
#include "ServerFSM.hpp"


class Server {
    public:
        Server(uint16_t port,
               unsigned int verbosity,
               const std::string &characterPath,
               const std::string &matchPath,
               const std::string &scenarioPath,
               std::map<std::string, std::string> additionalOptions);

        void run();

    private:
        uint16_t port;
        unsigned int verbosity;
        std::map<std::string, std::string> additionalOptions;
        spy::MatchConfig matchConfig;
        spy::scenario::Scenario scenarioConfig;
        std::vector<spy::character::CharacterDescription> characterDescriptions;
        MessageRouter router;

        ServerFSM fsm;
};


#endif //SERVER017_SERVER_HPP
