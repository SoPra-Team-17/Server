/**
 * @file GameFSM.hpp
 * @author ottojo
 * @brief State machine for game
 */
#ifndef SERVER017_GAMEFSM_HPP
#define SERVER017_GAMEFSM_HPP

#include <afsm/fsm.hpp>
#include <network/messages/GameOperation.hpp>
#include "Events.hpp"
#include "ServerUtil.hpp"
#include "Server.hpp"
#include "util/Player.hpp"

class GameFSM : public afsm::def::state_machine<GameFSM> {
    public:

        struct choicePhase : state<choicePhase> {
        };
        struct equipPhase : state<equipPhase> {
        };

        struct gamePhase : state_machine<gamePhase> {

            spy::util::UUID activeCharacter; //!< Set in roundInit state and waitingForOperation internal transition

            struct roundInit : state<roundInit> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &) {
                    spdlog::info("Entering state roundInit");
                    // TODO Zugreihenfolge initialisieren, Cocktails verteilen
                }
            };

            struct waitingForOperation : state<waitingForOperation> {

                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    spdlog::info("Entering state waitingForOperation");
                    spdlog::info("Requesting GameOperation for character {}", fsm.activeCharacter);
                    // TODO: find player owning activeCharacter
                    auto activePlayer = Player::one;
                    spdlog::info("Requesting GameOperation from player {}", activePlayer);
                    // TODO properly request Operation
                    root_machine(fsm).router.sendMessage(static_cast<int>(activePlayer),
                                                          spy::network::messages::RequestGameOperation());
                }

                using internal_transitions = transition_table <
                in<spy::network::messages::GameOperation, guards::operationValid, actions::nextCharacter>
                >;
            };

            using initial_state = roundInit;

            using transitions = transition_table <
            // Start        Event                        Next
            tr<roundInit,   events::roundInitDone,       waitingForOperation>
            >;
        };

        struct gameOver : state<gameOver> {
        };

        using initial_state = choicePhase;

        template<typename FSM, typename Event>
        void on_enter(Event &&, FSM &) {
            spdlog::info("Entering Game State");
        }

        using transitions = transition_table <
        // Start        Event                        Next
        tr<choicePhase, events::choicePhaseFinished, equipPhase>,
        tr<equipPhase,  events::equipPhaseFinished,  gamePhase>,
        tr<gamePhase,   events::gameFinished,        gameOver>
        >;
};

#endif //SERVER017_GAMEFSM_HPP
