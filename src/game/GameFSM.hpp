/**
 * @file GameFSM.hpp
 * @author ottojo
 * @brief State machine for game
 */

#ifndef SERVER017_GAMEFSM_HPP
#define SERVER017_GAMEFSM_HPP

#include <afsm/fsm.hpp>
#include <network/messages/GameOperation.hpp>
#include <datatypes/character/CharacterInformation.hpp>
#include <network/messages/ItemChoice.hpp>
#include <Actions.hpp>
#include "Events.hpp"
#include "Guards.hpp"
#include "util/Player.hpp"
#include "spdlog/fmt/ostr.h"
#include "OperationHandling.hpp"
#include "util/ChoiceSet.hpp"
#include "ChoicePhaseFSM.hpp"

class GameFSM : public afsm::def::state_machine<GameFSM> {
    public:
        ChoicePhase choicePhase;

        struct equipPhase : state<equipPhase> {
            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &) {
                spdlog::info("Entering equip phase");
            }
        };

        struct gamePhase : state_machine<gamePhase> {

            spy::util::UUID activeCharacter; //!< Set in roundInit state and waitingForOperation internal transition

            std::deque<spy::util::UUID> remainingCharacters; //!< Characters that have not made a move this round

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &) {
                spdlog::info("Initial entering to game phase");

                //spy::gameplay::State &gameState = root_machine(fsm).gameState;
                // TODO: add non chosen characters as NPCs
            }

            /**
             * Last operation + resulting operations (Exfiltration)
             */
            std::vector<std::shared_ptr<const spy::gameplay::BaseOperation>> operations;

            struct roundInit : state<roundInit> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    spdlog::info("Entering state roundInit");

                    for (const auto &c: root_machine(fsm).gameState.getCharacters()) {
                        fsm.remainingCharacters.push_back(c.getCharacterId());
                    }
                    std::shuffle(fsm.remainingCharacters.begin(), fsm.remainingCharacters.end(), root_machine(fsm).rng);

                    spdlog::info("Initialized round order:");
                    for (const auto &c: fsm.remainingCharacters) {
                        spdlog::info(c);
                    }
                    spy::gameplay::State &state = root_machine(fsm).gameState;
                    const spy::MatchConfig &matchConfig = root_machine(fsm).matchConfig;

                    using spy::util::RoundUtils;

                    RoundUtils::refillBarTables(state);
                    RoundUtils::updateFog(state);
                    RoundUtils::checkGadgetFailure(state, matchConfig);
                    RoundUtils::resetUpdatedMarker(state);
                    // TODO AP, BP festlegen



                    // TODO Cocktails verteilen
                    root_machine(fsm).process_event(events::roundInitDone{});
                }
            };

            /**
             * @brief In this state the server has requested an operation for activeCharacter and is waiting for response
             */
            struct waitingForOperation : state<waitingForOperation> {

                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &) {
                    spdlog::info("Entering state waitingForOperation");
                }

                // @formatter:off
                using internal_transitions = transition_table <
                // Event                                  Action                                                                                               Guard
                in<spy::network::messages::GameOperation, actions::multiple<actions::handleOperation, actions::broadcastState, actions::requestNextOperation>, and_<guards::operationValid, not_<guards::noCharactersRemaining>>>
                >;
                // @formatter:on
            };

            using initial_state = roundInit;


            // @formatter:off
            using transitions = transition_table <
            //  Start               Event                                  Next                 Action                    Guard
            tr<roundInit,           events::roundInitDone,                 waitingForOperation, actions::requestNextOperation>,
            tr<waitingForOperation, spy::network::messages::GameOperation, roundInit,           actions::handleOperation, guards::noCharactersRemaining>
            >;
            // @formatter:on
        };

        struct gameOver : state<gameOver> {
        };

        using initial_state = decltype(choicePhase);

        template<typename FSM, typename Event>
        void on_enter(Event &&, FSM &) {
            spdlog::info("Entering Game State");
        }

        // @formatter:off
        using transitions = transition_table <
        // Start                  Event                               Next
        tr<decltype(choicePhase), spy::network::messages::ItemChoice, equipPhase, actions::multiple<actions::handleChoice, actions::createCharacterSet>, and_<guards::choiceValid, guards::lastChoice>>,
        tr<equipPhase,            events::equipPhaseFinished,         gamePhase>,
        tr<gamePhase,             events::gameFinished,               gameOver>
        >;
        // @formatter:on
};

#endif //SERVER017_GAMEFSM_HPP
