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

constexpr unsigned int defaultMaxNPCs = 8;


class Server : public afsm::def::state_machine<Server> {
    public:
        Server(uint16_t port,
               unsigned int verbosity,
               const std::string &characterPath,
               const std::string &matchPath,
               const std::string &scenarioPath,
               std::map<std::string, std::string> additionalOptions);

        struct emptyLobby : state<emptyLobby> {
            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::debug("Entering state emptyLobby");

                root_machine(fsm).isIngame = false;
            }

            // @formatter:off
            using internal_transitions = transition_table <
            // Event                                  Action                  Guard
            in<spy::network::messages::Hello,         actions::HelloReply,    guards::isSpectator>
            >;
            // @formatter:on
        };

        struct waitFor2Player : state<waitFor2Player> {
            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &) {
                spdlog::debug("Entering state waitFor2Player");
            }

            // @formatter:off
            using internal_transitions = transition_table <
            // Event                                  Action                  Guard
            in<spy::network::messages::Hello,         actions::HelloReply,    guards::isSpectator>
            >;
            // @formatter:on
        };

        GameFSM game;

        using initial_state = emptyLobby;

        // @formatter:off
        using transitions = transition_table <
        // Start               Event                               Next            Action                                                               Guard
        tr<emptyLobby,         spy::network::messages::Hello,      waitFor2Player, actions::multiple<actions::InitializeSession, actions::HelloReply>, not_<guards::isSpectator>>,
        tr<waitFor2Player,     spy::network::messages::GameLeave,  emptyLobby>,
        tr<waitFor2Player,     spy::network::messages::Hello,      decltype(game), actions::multiple<actions::HelloReply, actions::StartGame>,          not_<guards::isSpectator>>,
        tr<GameFSM,            none,                               emptyLobby,     actions::closeGame,                                                  guards::gameOver>
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
         * This should be true when the current state is GameFSM::gamePhase
         * (direct checking not possible in guards::gameOver)
         */
        bool isIngame = false;

        /**
         * Known safe combinations (not indices) for both players
         */
        std::map<Player, std::set<int>> knownCombinations;

        /**
         * Client IDs for both players
         */
        std::map<Player, spy::util::UUID> playerIds;

        /**
         * Roles of the connected clients.
         */
        std::map<spy::util::UUID, spy::network::RoleEnum> clientRoles;

        /**
         * Names for both players
         */
        std::map<Player, std::string> playerNames;

        spy::util::UUID sessionId;
        std::random_device rd{};
        std::mt19937 rng{rd()};

        /**
         * UUID of the cat
         */
        spy::util::UUID catId = spy::util::UUID::generate();


        /**
         * UUID of the janitor
         */
        spy::util::UUID janitorId = spy::util::UUID::generate();

        /**
         * Holds all characters and gadgets currently available to choose from.
         */
        ChoiceSet choiceSet;

        unsigned int maxNumberOfNPCs = defaultMaxNPCs;

    private:
        const static std::map<unsigned int, spdlog::level::level_enum> verbosityMap;

        void configureLogging() const;
};


#endif //SERVER017_SERVER_HPP
