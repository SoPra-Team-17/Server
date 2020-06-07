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
#include <network/messages/RequestEquipmentChoice.hpp>
#include "Events.hpp"
#include "Guards.hpp"
#include "Actions.hpp"
#include "util/Player.hpp"
#include "spdlog/fmt/ostr.h"
#include "OperationHandling.hpp"
#include "util/ChoiceSet.hpp"
#include "ChoicePhaseFSM.hpp"
#include "EquipChoiceHandling.hpp"
#include "util/Timer.hpp"

class GameFSM : public afsm::def::state_machine<GameFSM> {
    public:
        ChoicePhase choicePhase;

        struct equipPhase : state<equipPhase> {
            // Maps from clientId to the vector of chosen character uuids
            using CharacterMap = std::map<spy::util::UUID, std::vector<spy::util::UUID>>;
            // Maps from clientId to the vector of chosen gadget types
            using GadgetMap = std::map<spy::util::UUID, std::vector<spy::gadget::GadgetEnum>>;


            CharacterMap chosenCharacters;              ///< Stores the character choice of the players
            GadgetMap chosenGadgets;                    ///< Stores the gadget choice of the players
            std::map<spy::util::UUID, bool> hasChosen;  ///< Stores whether the client has already sent his equip choice

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Entering equip phase");

                const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
                MessageRouter &router = root_machine(fsm).router;

                auto idP1 = playerIds.at(Player::one);
                auto idP2 = playerIds.at(Player::two);

                hasChosen[idP1] = false;
                hasChosen[idP2] = false;

                spy::network::messages::RequestEquipmentChoice messageP1(idP1, chosenCharacters.at(idP1),
                                                                         chosenGadgets.at(idP1));
                spy::network::messages::RequestEquipmentChoice messageP2(idP2, chosenCharacters.at(idP2),
                                                                         chosenGadgets.at(idP2));

                router.sendMessage(messageP1);
                spdlog::info("Sending request for equipment choice to player1 ({})", idP1);
                router.sendMessage(messageP2);
                spdlog::info("Sending request for equipment choice to player2 ({})", idP2);
            }

            // @formatter:off
            using internal_transitions = transition_table <
            //  Event                                   Action                                                                  Guard
            in<spy::network::messages::EquipmentChoice, actions::handleEquipmentChoice, and_<not_<guards::lastEquipmentChoice>, guards::equipmentChoiceValid>>
            >;
            // @formatter:on
        };

        struct gamePhase : state_machine<gamePhase> {

            spy::util::UUID activeCharacter; //!< Set in roundInit state and waitingForOperation internal transition

            std::deque<spy::util::UUID> remainingCharacters; //!< Characters that have not made a move this round

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Initial entering to game phase");

                root_machine(fsm).isIngame = true;

                spy::gameplay::State &gameState = root_machine(fsm).gameState;
                spy::MatchConfig &config = root_machine(fsm).matchConfig;
                std::mt19937 &rng = root_machine(fsm).rng;
                auto &knownCombinations = root_machine(fsm).knownCombinations;

                knownCombinations[Player::one] = {};
                knownCombinations[Player::two] = {};

                std::vector<unsigned int> safeIndexes;

                gameState.getMap().forAllFields([&safeIndexes](const spy::scenario::Field &f) {
                    if (f.getFieldState() == spy::scenario::FieldStateEnum::SAFE) {
                        safeIndexes.push_back(safeIndexes.size() + 1);
                    }
                });

                std::shuffle(safeIndexes.begin(), safeIndexes.end(), root_machine(fsm).rng);

                auto indexIterator = safeIndexes.begin();

                // Initialize roulette tables with random amount of chips, safes with an index
                gameState.getMap().forAllFields([&rng, &config, &indexIterator](spy::scenario::Field &field) {
                    if (field.getFieldState() == spy::scenario::FieldStateEnum::ROULETTE_TABLE) {
                        std::uniform_int_distribution<unsigned int> randChips(config.getMinChipsRoulette(),
                                                                              config.getMaxChipsRoulette());

                        field.setChipAmount(randChips(rng));
                    } else if (field.getFieldState() == spy::scenario::FieldStateEnum::SAFE) {
                        field.setSafeIndex(*indexIterator);
                        indexIterator++;
                    }
                });

                // Randomly distribute characters
                spdlog::info("Distributing characters");
                for (auto &character: gameState.getCharacters()) {
                    auto randomField = spy::util::GameLogicUtils::getRandomCharacterFreeMapPoint(gameState);
                    if (!randomField.has_value()) {
                        spdlog::critical("No field to place character");
                        std::exit(1);
                    }
                    spdlog::debug("Placing {} at {}", character.getName(), fmt::json(randomField.value()));
                    character.setCoordinates(randomField.value());
                }

                // place the cat on a random field
                auto randomField = spy::util::GameLogicUtils::getRandomCharacterFreeMapPoint(gameState);
                if (!randomField.has_value()) {
                    spdlog::critical("No field to place the white cat");
                    std::exit(1);
                }
                spdlog::debug("Placing white cat at {}", fmt::json(randomField.value()));
                gameState.setCatCoordinates(randomField.value());
            }

            template<typename FSM, typename Event>
            void on_exit(Event &&, FSM &fsm) {
                spdlog::debug("Exiting state gamePhase");
                root_machine(fsm).isIngame = false;
            }

            /**
             * Last operation + resulting operations (Exfiltration)
             */
            std::vector<std::shared_ptr<const spy::gameplay::BaseOperation>> operations;

            struct roundInit : state<roundInit> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    using spy::util::GameLogicUtils;
                    using spy::scenario::FieldStateEnum;
                    using spy::util::RoundUtils;
                    using spy::gadget::Gadget;
                    using spy::gadget::GadgetEnum;
                    using spy::character::FactionEnum;

                    auto &characters = root_machine(fsm).gameState.getCharacters();
                    spy::gameplay::State &state = root_machine(fsm).gameState;
                    const spy::MatchConfig &matchConfig = root_machine(fsm).matchConfig;
                    state.incrementRoundCounter();

                    spdlog::info("Entering state roundInit for round {}", state.getCurrentRound());

                    // janitor is only active after the round limit was reached
                    if (state.getCurrentRound() >= matchConfig.getRoundLimit()) {
                        // if the janitor wasn't previously on the map, this is the first round with special mechanics
                        if (!state.getJanitorCoordinates().has_value()) {
                            auto randomField = GameLogicUtils::getRandomCharacterFreeMapPoint(state);

                            if (!randomField.has_value()) {
                                spdlog::critical("No field to place the janitor");
                                throw std::invalid_argument("No field to place the janitor");
                            }
                            spdlog::debug("Initial placement of the janitor at {}", fmt::json(randomField.value()));
                            state.setJanitorCoordinates(randomField.value());

                            // all NPCs leave the casino
                            state.removeAllNPCs();
                        }

                        fsm.remainingCharacters.push_back(root_machine(fsm).janitorId);
                    }

                    for (const auto &c: characters) {
                        if (c.getCoordinates().has_value()) {
                            fsm.remainingCharacters.push_back(c.getCharacterId());
                        }
                    }
                    fsm.remainingCharacters.push_back(root_machine(fsm).catId);

                    std::shuffle(fsm.remainingCharacters.begin(), fsm.remainingCharacters.end(), root_machine(fsm).rng);

                    fsm.activeCharacter = {};

                    spdlog::info("Initialized round order:");
                    for (const auto &uuid: fsm.remainingCharacters) {
                        if (uuid == root_machine(fsm).catId) {
                            spdlog::info("White cat");
                        } else if (uuid == root_machine(fsm).janitorId) {
                            spdlog::info("Janitor");
                        } else {
                            spdlog::info("{} \t({})", characters.findByUUID(uuid)->getName(), uuid);
                        }
                    }

                    RoundUtils::refillBarTables(state);
                    RoundUtils::updateFog(state);
                    RoundUtils::checkGadgetFailure(state, matchConfig);
                    RoundUtils::resetUpdatedMarker(state);
                    for (auto &character: state.getCharacters()) {
                        RoundUtils::determinePoints(character);
                    }

                    root_machine(fsm).process_event(events::roundInitDone{});
                }

                using internal_transitions = transition_table <
                // Event                          Action                                                               Guard
                in<spy::network::messages::Hello, actions::multiple<actions::HelloReply, actions::sendSpectatorState>, guards::isSpectator>>;
                // @formatter:on
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
                // Event                                  Action                                                                                                  Guard
                in<spy::network::messages::GameOperation, actions::multiple<actions::handleOperation, actions::broadcastState, actions::requestNextOperation>,    guards::operationValid>,
                in<events::triggerNPCmove,                actions::multiple<actions::generateNPCMove>>,
                in<events::triggerCatMove,                actions::multiple<actions::executeCatMove, actions::broadcastState, actions::requestNextOperation>>,
                in<events::triggerJanitorMove,            actions::multiple<actions::executeJanitorMove, actions::broadcastState, actions::requestNextOperation>>,
                in<spy::network::messages::Hello, actions::multiple<actions::HelloReply, actions::sendSpectatorState>,                                            guards::isSpectator>>;
                // @formatter:on
            };

            struct paused : state<paused> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    spdlog::info("Entering state paused, serverEnforced={}", serverEnforced);
                    spy::MatchConfig matchConfig = root_machine(fsm).matchConfig;
                    if (not serverEnforced and matchConfig.getPauseLimit().has_value()) {
                        spdlog::info("Starting pause timer for {} seconds", matchConfig.getPauseLimit().value());
                        timer.restart(std::chrono::seconds{matchConfig.getPauseLimit().value()}, [&fsm]() {
                            spdlog::info("Pause time limit reached, unpausing.");
                            root_machine(fsm).process_event(events::forceUnpause{});
                        });
                    }
                }

                bool serverEnforced = false;
                Timer timer;

                using internal_transitions = transition_table <
                // Event                          Action                                                               Guard
                in<spy::network::messages::Hello, actions::multiple<actions::HelloReply, actions::sendSpectatorState>, guards::isSpectator>>;
                // @formatter:on
            };

            using initial_state = roundInit;


            // @formatter:off
            using transitions = transition_table <
            //  Start               Event                                     Next                 Action                                                                      Guard
            tr<roundInit,           events::roundInitDone,                    waitingForOperation, actions::multiple<actions::broadcastState, actions::requestNextOperation>>,
            tr<waitingForOperation, events::roundDone,                        roundInit>,
            // Player requested pause
            tr<waitingForOperation, spy::network::messages::RequestGamePause, paused,              actions::pauseGame,                                                          guards::isPauseRequest>,
            // Player requested unpause
            tr<paused,              spy::network::messages::RequestGamePause, waitingForOperation, actions::unpauseGame,                                                        guards::isUnPauseRequest>,
            // Server forced unpause
            tr<paused,              events::forceUnpause,                     waitingForOperation, actions::unpauseGame>
            >;
        };

        using initial_state = decltype(choicePhase);

        template<typename FSM, typename Event>
        void on_enter(Event &&, FSM &) {
            spdlog::info("Entering Game State");
        }

        // @formatter:off
        using transitions = transition_table <
        // Start                  Event                                    Next        Action                                                                 Guard
        tr<decltype(choicePhase), spy::network::messages::ItemChoice,      equipPhase, actions::multiple<actions::handleChoice, actions::createCharacterSet>, and_<guards::lastChoice, guards::choiceValid>>,
        tr<equipPhase,            spy::network::messages::EquipmentChoice, gamePhase,  actions::handleEquipmentChoice,                                        and_<guards::lastEquipmentChoice, guards::equipmentChoiceValid>>
        >;
        // @formatter:on
};

#endif //SERVER017_GAMEFSM_HPP
