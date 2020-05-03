/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   02.05.2020 (creation)
 * @brief  Declaration of the server class.
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

namespace events {
    struct choicePhaseFinished {};
    struct equipPhaseFinished {};
    struct gameFinished {};
    struct roundInitDone {};
}

class Server : afsm::def::state_machine<Server> {
    public:
        Server(uint16_t port,
               unsigned int verbosity,
               const std::string &characterPath,
               const std::string &matchPath,
               const std::string &scenarioPath,
               std::map<std::string, std::string> additionalOptions);

        struct emptyLobby : state<emptyLobby>{};
        struct waitFor2Player : state<waitFor2Player>{};
        struct game : state_machine<game> {
            // Ziel: Auslagern in game.hpp

            struct choicePhase : state<choicePhase>{};
            struct equipPhase : state<equipPhase>{};

            struct gamePhase : state_machine<gamePhase>{
                struct roundInit : state<roundInit>{};                          //Cocktails + Zugreihenfolge
                struct waitingForOperation : state<waitingForOperation>{};

                using initial_state = roundInit;

                using transitions  = transition_table<
                    // Start        Event                        Next
                    tr<roundInit,   events::roundInitDone,       waitingForOperation>
                >;
            };

            struct gameOver : state<gameOver>{};

            using initial_state = choicePhase;

            using transitions = transition_table<
                // Start        Event                        Next
                tr<choicePhase, events::choicePhaseFinished, equipPhase>,
                tr<equipPhase,  events::equipPhaseFinished,  gamePhase>,
                tr<gamePhase,   events::gameFinished,        gameOver>
            >;
        };

        using transitions = transition_table<
            // Start            Event                               Next
            tr<emptyLobby,      spy::network::messages::Hello,      waitFor2Player>,
            tr<waitFor2Player,  spy::network::messages::GameLeave,  emptyLobby>,
            tr<waitFor2Player,  spy::network::messages::Hello,      game>
        >;

    private:
        uint16_t port;
        unsigned int verbosity;
        std::map<std::string, std::string> additionalOptions;
        spy::MatchConfig matchConfig;
        spy::scenario::Scenario scenarioConfig;
        std::vector<spy::character::CharacterDescription> characterDescriptions;
        MessageRouter router;

        void configureLogging() const;
};


#endif //SERVER017_SERVER_HPP
