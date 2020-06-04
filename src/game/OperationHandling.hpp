/**
 * @file OperationHandling.hpp
 * @brief FSM actions relating to GameOperation requesting and handling
 * @author ottojo
 */

#ifndef SERVER017_OPERATIONHANDLING_HPP
#define SERVER017_OPERATIONHANDLING_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameStatus.hpp>
#include <util/RoundUtils.hpp>
#include <gameLogic/generation/ActionGenerator.hpp>
#include <gameLogic/execution/ActionExecutor.hpp>
#include "util/Format.hpp"
#include "util/Player.hpp"
#include "util/Operation.hpp"
#include "util/Util.hpp"

namespace actions {
    /**
     * @brief Action to apply a valid operation to the state
     */
    struct handleOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &, TargetState &) {
            using spy::network::messages::GameOperation;
            using spy::gameplay::State;

            spdlog::info("Handling some operation");

            State &state = root_machine(fsm).gameState;
            auto &knownCombinations = root_machine(fsm).knownCombinations;

            const GameOperation &operationMessage = std::forward<GameOperation>(e);

            // update the state with the current players safe combination knowledge
            auto activeCharacter = state.getCharacters().findByUUID(fsm.activeCharacter);

            // Find player owning character
            Player player;
            if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER1) {
                player = Player::one;
                spdlog::info("Character belongs to player one");
            } else if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER2) {
                player = Player::two;
                spdlog::info("Character belongs to player two");
            } else {
                spdlog::error("Character is NPC");
            }

            state.setKnownSafeCombinations(knownCombinations.at(player));


            executeOperation(operationMessage.getOperation(),
                             state,
                             root_machine(fsm).matchConfig,
                             fsm.operations);

            // copy the potentially changed known combinations back to the map
            knownCombinations[player] = state.getMySafeCombinations();
        }
    };

    /**
     * Broadcasts the current state to the players and the spectators. Spectators receive no information about
     * the known safe combinations, the players receive their respective knowledge.
     */
    struct broadcastState {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Broadcasting state");
            MessageRouter &router = root_machine(fsm).router;
            const auto &playerIds = root_machine(fsm).playerIds;
            const auto &clientRoles = root_machine(fsm).clientRoles;

            // spectators
            spy::gameplay::State stateSpec = root_machine(fsm).gameState;
            bool gameOver = spy::util::RoundUtils::isGameOver(stateSpec);
            stateSpec.setKnownSafeCombinations({});
            spy::network::messages::GameStatus messageSpec(
                    {}, // filled out by the message router
                    fsm.activeCharacter,
                    fsm.operations,
                    stateSpec,
                    gameOver);

            // send the spectator state to all spectators
            for (const auto &[uuid, role] : clientRoles) {
                if (role == spy::network::RoleEnum::SPECTATOR) {
                    router.sendMessage(uuid, messageSpec);
                }
            }

            // players
            for (const auto &player : {Player::one, Player::two}) {
                spy::gameplay::State state = root_machine(fsm).gameState;
                state.setKnownSafeCombinations(root_machine(fsm).knownCombinations.at(player));
                spy::network::messages::GameStatus message(
                        playerIds.at(player),
                        fsm.activeCharacter,
                        fsm.operations,
                        state,
                        gameOver);

                router.sendMessage(message);
            }

            fsm.operations.clear();
        }
    };

    /**
     * @brief Generates a NPC action and posts it to the FSM
     */
    struct generateNPCMove {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Generating NPC action");

            using spy::gameplay::ActionGenerator;
            auto npcAction = ActionGenerator::generateNPCAction(root_machine(fsm).gameState,
                                                                fsm.activeCharacter,
                                                                root_machine(fsm).matchConfig);

            if (npcAction != nullptr) {
                spy::network::messages::GameOperation op{{}, npcAction};
                root_machine(fsm).process_event(op);
            } else {
                spdlog::error("Generating NPC action failed.");
            }
        }
    };

    /**
     * Chooses next Character and requests Operation.
     * Emits events::triggerNPCmove, triggerCatMove, triggerJanitorMove, roundDone
     */
    struct requestNextOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            spy::gameplay::State &state = root_machine(fsm).gameState;

            if (spy::util::RoundUtils::isGameOver(state)) {
                // There may still be characters remaining, but the game has been won with the last action.
                // We do not have to request a new operation, and abort early.
                spdlog::info("Skipping requestNextOperation because game is already over.");
                return;
            }

            // Check if the current character has retired
            bool isRetire = false;
            if constexpr(std::is_same<Event, spy::network::messages::GameOperation>::value) {
                const spy::network::messages::GameOperation &operation = event;
                if (operation.getOperation()->getType() == spy::gameplay::OperationEnum::RETIRE) {
                    isRetire = true;
                }
            }

            if (isRetire or not Util::canMoveAgain(*state.getCharacters().findByUUID(fsm.activeCharacter))) {
                spdlog::info("Character done. Choosing next.");
                if (fsm.remainingCharacters.empty()) {
                    spdlog::info("No characters remaining. Sending events::roundDone to FSM");
                    root_machine(fsm).process_event(events::roundDone{});
                    return;
                }

                // Characters are remaining, take next:
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();
                spdlog::info("Chose {} as next character.", fsm.activeCharacter);

                // Check if special handling for Cat and Janitor is required
                if (fsm.activeCharacter == root_machine(fsm).catId) {
                    spdlog::debug("requestNextOperation determined that next character is the white cat"
                                  "-> Not requesting, triggering cat move instead.");

                    root_machine(fsm).process_event(events::triggerCatMove{});
                    return;
                } else if (fsm.activeCharacter == root_machine(fsm).janitorId) {
                    spdlog::debug("requestNextOperation determined that next character is the janitor"
                                  "-> Not requesting, triggering janitor move instead.");

                    root_machine(fsm).process_event(events::triggerJanitorMove{});
                    return;
                }

                // Regular character
                spy::character::CharacterSet::iterator nextCharacter =
                        state.getCharacters().getByUUID(fsm.activeCharacter);
                spdlog::info("Next character is regular character {}", nextCharacter->getName());

                // Determine which player the character belongs to
                std::optional<Player> activePlayer;
                switch (nextCharacter->getFaction()) {
                    case spy::character::FactionEnum::PLAYER1:
                        activePlayer = Player::one;
                        break;
                    case spy::character::FactionEnum::PLAYER2:
                        activePlayer = Player::two;
                        break;
                    default:
                        activePlayer = std::nullopt;
                }

                if (activePlayer.has_value()) {
                    spy::network::messages::RequestGameOperation request{
                            root_machine(fsm).playerIds.find(activePlayer.value())->second,
                            fsm.activeCharacter
                    };
                    MessageRouter &router = root_machine(fsm).router;
                    spdlog::info("Requesting Operation from player {}", activePlayer.value());
                    router.sendMessage(request);
                } else {
                    spdlog::debug("requestNextOperation determined that next character is not a PC"
                                  "-> Not requesting, triggering NPC move instead.");
                    root_machine(fsm).process_event(events::triggerNPCmove{});
                }
            }
        }
    };

    /**
     * @brief Generates and executes a cat movement
     */
    struct executeCatMove {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Generating cat action");

            using spy::gameplay::State;
            using spy::gameplay::ActionExecutor;
            using spy::gameplay::ActionGenerator;
            using spy::gameplay::CatAction;

            State &state = root_machine(fsm).gameState;

            auto catAction = ActionGenerator::generateCatAction(state);

            ActionExecutor::executeCat(state, *std::dynamic_pointer_cast<const CatAction>(catAction));
        }
    };

    /**
     * @brief Generates and executes a janitor movement
     */
    struct executeJanitorMove {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Generating janitor action");
            using spy::gameplay::State;
            using spy::gameplay::ActionExecutor;
            using spy::gameplay::ActionGenerator;
            using spy::gameplay::JanitorAction;

            State &state = root_machine(fsm).gameState;

            auto janitorAction = ActionGenerator::generateJanitorAction(state);

            ActionExecutor::executeJanitor(state, *std::dynamic_pointer_cast<const JanitorAction>(janitorAction));
        }
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
