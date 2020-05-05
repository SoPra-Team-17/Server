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
#include "Events.hpp"
#include "ServerUtil.hpp"
#include "Server.hpp"
#include "util/Player.hpp"
#include "spdlog/fmt/ostr.h"
#include "OperationHandling.hpp"

class GameFSM : public afsm::def::state_machine<GameFSM> {
    public:

        struct choicePhase : state<choicePhase> {

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Entering choice phase");
                spy::gameplay::State &gameState = root_machine(fsm).gameState;
                std::vector<spy::character::CharacterInformation> &characterInformations =
                        root_machine(fsm).characterInformations;

                // TODO: choice phase, do not just use every character available
                spy::character::CharacterSet characters;
                for (const auto &c: characterInformations) {
                    auto character = spy::character::Character{c.getCharacterId(), c.getName()};
                    character.setProperties(
                            std::set<spy::character::PropertyEnum>{c.getFeatures().begin(), c.getFeatures().end()});
                    characters.insert(character);
                }

                gameState = spy::gameplay::State{1,
                                                 gameState.getMap(),
                                                 gameState.getMySafeCombinations(),
                                                 characters,
                                                 gameState.getCatCoordinates(),
                                                 gameState.getJanitorCoordinates()};
            }
        };

        struct equipPhase : state<equipPhase> {
            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &) {
                spdlog::info("Entering equip phase");
            }
        };

        struct gamePhase : state_machine<gamePhase> {

            spy::util::UUID activeCharacter; //!< Set in roundInit state and waitingForOperation internal transition

            std::deque<spy::util::UUID> remainingCharacters; //!< Characters that have not made a move this round

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
                    // TODO Cocktails verteilen
                    root_machine(fsm).process_event(events::roundInitDone{});
                }
            };

            struct waitingForOperation : state<waitingForOperation> {

                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &) {
                    spdlog::info("Entering state waitingForOperation");
                }

                using internal_transitions = transition_table <
                in<spy::network::messages::GameOperation, actions::handleOperationAndRequestNext, and_ < not_ < guards::noCharactersRemaining>, guards::operationValid>>
                >;
            };

            using initial_state = roundInit;

            using transitions = transition_table <
            //  Start           Event                       Next            Action  Guard
            tr<roundInit, events::roundInitDone, waitingForOperation, actions::requestNextOperation>,
            tr<waitingForOperation, spy::network::messages::GameOperation, roundInit, actions::handleOperation, guards::noCharactersRemaining>
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
        tr<equipPhase, events::equipPhaseFinished, gamePhase>,
        tr<gamePhase, events::gameFinished, gameOver>
        >;
};

#endif //SERVER017_GAMEFSM_HPP
