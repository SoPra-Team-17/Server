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

            Timer playerOneReconnectTimer;
            Timer playerTwoReconnectTimer;

            template<typename FSM>
            static void limitReached(FSM &fsm, Player p) {
                spdlog::warn("Player {} reconnect limit in equip phase reached, closing game", p);
                root_machine(fsm).process_event(
                        events::forceGameClose{
                                Util::opponentOf(p),
                                spy::statistics::VictoryEnum::VICTORY_BY_LEAVE});
            }

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Entering equip phase");

                const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
                MessageRouter &router = root_machine(fsm).router;

                for (const auto &player: {Player::one, Player::two}) {
                    hasChosen[root_machine(fsm).playerIds.at(player)] = false;
                    auto playerId = playerIds.at(player);
                    spy::network::messages::RequestEquipmentChoice requestMessage{
                            playerId,
                            chosenCharacters.at(playerId),
                            chosenGadgets.at(playerId)};

                    router.sendMessage(requestMessage);
                    spdlog::info("Sending request for equipment choice to player {} ({})", player, playerId);
                }
            }

            // @formatter:off
            using internal_transitions = transition_table <
            //  Event                                   Action                                                                                                                                                                               Guard
            in<spy::network::messages::EquipmentChoice, actions::handleEquipmentChoice,                                                                                                                                                      and_<not_<guards::lastEquipmentChoice>, guards::equipmentChoiceValid>>,
            in<spy::network::messages::EquipmentChoice, actions::multiple<actions::replyWithError<spy::network::ErrorTypeEnum::ILLEGAL_MESSAGE>, actions::closeConnectionToClient, actions::broadcastGameLeft, actions::emitForceGameClose>, not_<guards::equipmentChoiceValid>>,
            in<spy::network::messages::Reconnect,       actions::multiple<actions::stopReconnectTimer, actions::repeatEquipmentRequest>>,
            in<events::playerDisconnect,                actions::startChoicePhaseTimer>
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
                in<spy::network::messages::Hello, actions::multiple<actions::HelloReply, actions::broadcastState>, guards::isSpectator>>;
                // @formatter:on
            };

            /**
             * @brief In this state the server has requested an operation for activeCharacter and is waiting for response
             */
            struct waitingForOperation : state<waitingForOperation> {

                Timer turnPhaseTimer;

                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &) {
                    spdlog::info("Entering state waitingForOperation");
                }

                // @formatter:off
                using internal_transitions = transition_table <
                // Event                                  Action                                                                                                                                                                               Guard
                in<spy::network::messages::GameOperation, actions::multiple<actions::handleOperation, actions::broadcastState, actions::requestNextOperation>,                                                                                 guards::operationValid>,
                in<events::skipOperation,                 actions::multiple<actions::broadcastState, actions::requestNextOperation>>,
                in<spy::network::messages::GameOperation, actions::multiple<actions::replyWithError<spy::network::ErrorTypeEnum::ILLEGAL_MESSAGE>, actions::closeConnectionToClient, actions::broadcastGameLeft, actions::emitForceGameClose>, not_<guards::operationValid>>,
                in<events::triggerNPCmove,                actions::multiple<actions::generateNPCMove>>,
                in<events::triggerCatMove,                actions::multiple<actions::executeCatMove, actions::broadcastState, actions::requestNextOperation>>,
                in<events::triggerJanitorMove,            actions::multiple<actions::executeJanitorMove, actions::broadcastState, actions::requestNextOperation>>,
                in<spy::network::messages::Hello,         actions::multiple<actions::HelloReply, actions::broadcastState>,                                                                                                                     guards::isSpectator>>;
                // @formatter:on
            };

            struct paused : state<paused> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    spdlog::info("Entering state paused, serverEnforced={}", serverEnforced);
                    spy::MatchConfig matchConfig = root_machine(fsm).matchConfig;
                    if (not serverEnforced and matchConfig.getPauseLimit().has_value()) {
                        spdlog::info("Starting pause timer for {} seconds", matchConfig.getPauseLimit().value());
                        pauseLimitTimer.restart(std::chrono::seconds{matchConfig.getPauseLimit().value()}, [&fsm]() {
                            spdlog::info("Pause time limit reached, unpausing.");
                            root_machine(fsm).process_event(events::forceUnpause{});
                        });
                    }
                }

                bool serverEnforced = false;

                Timer pauseLimitTimer;

                Timer playerOneReconnectTimer;
                Timer playerTwoReconnectTimer;

                std::chrono::system_clock::duration pauseTimeRemaining{0};

                // @formatter:off
                using internal_transitions = transition_table <
                // Event                              Action                                                                        Guard
                in<spy::network::messages::Hello,     actions::multiple<actions::HelloReply, actions::broadcastState>,              guards::isSpectator>,
                // Another player disconnects, stay in pause
                in<events::playerDisconnect,          actions::startReconnectTimer>,
                // A player reconnects, but one is still disconnected
                in<spy::network::messages::Reconnect, actions::stopReconnectTimer,                                                  guards::bothDisconnected>,
                // A player reconnects, and before the disconnect(s) there was a normal pause which we have to continue
                in<spy::network::messages::Reconnect, actions::multiple<actions::stopReconnectTimer, actions::revertToNormalPause>, and_<guards::pauseTimeRemaining, not_<guards::bothDisconnected>>>>;
                // @formatter:on
            };

            using initial_state = roundInit;


            // @formatter:off
            using transitions = transition_table <
            //  Start               Event                                     Next                 Action                                                                      Guard
            tr<roundInit,           events::roundInitDone,                    waitingForOperation, actions::multiple<actions::broadcastState, actions::requestNextOperation>>,
            tr<waitingForOperation, events::roundDone,                        roundInit>,
            // Player requested pause
            tr<waitingForOperation, spy::network::messages::RequestGamePause, paused,              actions::pauseGame<false>,                                                  guards::isPauseRequest>,
            // Player requested unpause
            tr<paused,              spy::network::messages::RequestGamePause, waitingForOperation, actions::unpauseGame,                                                       guards::isUnPauseRequest>,
            // Server forced unpause
            tr<paused,              events::forceUnpause,                     waitingForOperation, actions::unpauseGame>,
            // Force pause when player disconnects
            tr<waitingForOperation, events::playerDisconnect,                 paused,              actions::multiple<actions::pauseGame<true>, actions::startReconnectTimer>>,
            // Unpause if a player reconnects and not both players are disconnected and no pause time remaining
            tr<paused,              spy::network::messages::Reconnect,        waitingForOperation, actions::multiple<actions::sendReconnectGameStart, actions::broadcastState, actions::unpauseGame, actions::requestNextOperation>, and_<not_<guards::pauseTimeRemaining>, not_<guards::bothDisconnected>>>
            >;
            // @formatter:on
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

        using internal_transitions = transition_table <
        // Event                          Action                                                                 Guard
        in<spy::network::messages::Hello, actions::replyWithError<spy::network::ErrorTypeEnum::ALREADY_SERVING>, guards::isPlayer>
        >;
        // @formatter:on
};

#endif //SERVER017_GAMEFSM_HPP
