/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   02.05.2020 (creation)
 * @brief  Declaration of the server class and state maching.
 */

#ifndef SERVER017_SERVER_HPP
#define SERVER017_SERVER_HPP

#include <afsm/fsm.hpp>
#include "datatypes/matchconfig/MatchConfig.hpp"
#include "datatypes/scenario/Scenario.hpp"
#include "datatypes/character/CharacterDescription.hpp"
#include "network/MessageRouter.hpp"
#include "network/messages/Hello.hpp"
#include "network/messages/GameLeave.hpp"
#include <Events.hpp>
#include <game/GameFSM.hpp>
#include <random>
#include<Actions.hpp>


class Server : public afsm::def::state_machine<Server> {
    public:
        Server(uint16_t port,
               unsigned int verbosity,
               const std::string &characterPath,
               const std::string &matchPath,
               const std::string &scenarioPath,
               std::map<std::string, std::string> additionalOptions);

        struct emptyLobby : state<emptyLobby> {
        };

        struct waitFor2Player : state<waitFor2Player> {
            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &) {
                spdlog::debug("Entering state waitFor2Player");
            }
        };

        GameFSM game;

        using initial_state = emptyLobby;

        // @formatter:off
        using transitions = transition_table <
        // Start            Event                               Next            Action                                                               Guard
        tr<emptyLobby,      spy::network::messages::Hello,      waitFor2Player, actions::multiple<actions::InitializeSession, actions::HelloReply>>,
        tr<waitFor2Player,  spy::network::messages::GameLeave,  emptyLobby>,
        tr<waitFor2Player,  spy::network::messages::Hello,      decltype(game), actions::multiple<actions::HelloReply, actions::StartGame>>,
        tr<GameFSM,         none,                               emptyLobby,     actions::closeGame,                                                  guards::gameOver>
        >;
        // @formatter:on

        unsigned int verbosity;
        std::map<std::string, std::string> additionalOptions;
        spy::MatchConfig matchConfig;
        spy::scenario::Scenario scenarioConfig;

        /**
         * Characters from configuration file + UUIDs
         */
        std::vector<spy::character::CharacterInformation> characterInformations;

        MessageRouter router;

        /**
         * Current game state, contains characters and faction information after successful equipment phase.
         */
        spy::gameplay::State gameState;

        /**
         * Safe combinations by safe index
         */
        std::map<unsigned int, int> safeCombinations;

        /**
         * Known safe combinations (not indices) for both players
         */
        std::map<Player, std::set<int>> knownCombinations;

        /**
         * Client IDs for both players
         */
        std::map<Player, spy::util::UUID> playerIds;

        /**
         * Names for both players
         */
        std::map<Player, std::string> playerNames;

        spy::util::UUID sessionId;
        std::random_device rd{};
        std::mt19937 rng{rd()};

        /**
         * Holds all characters and gadgets currently available to choose from.
         */
        ChoiceSet choiceSet;

    private:
        const static std::map<unsigned int, spdlog::level::level_enum> verbosityMap;

        void configureLogging() const;
};


#endif //SERVER017_SERVER_HPP
