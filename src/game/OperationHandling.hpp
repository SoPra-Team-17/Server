/**
 * @file OperationHandling.hpp
 * @brief FSM actions relating to GameOperation requesting and handling
 * @author ottojo
 */

#ifndef SERVER017_OPERATIONHANDLING_HPP
#define SERVER017_OPERATIONHANDLING_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameStatus.hpp>
#include <network/messages/Strike.hpp>
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
        void operator()(Event &&e, FSM &fsm, SourceState &source, TargetState &) {
            using spy::network::messages::GameOperation;
            using spy::gameplay::State;

            spdlog::info("Handling some operation");

            spdlog::info("Stopping turnPhase timer");
            source.turnPhaseTimer.stop();

            State &state = root_machine(fsm).gameState;
            auto &knownCombinations = root_machine(fsm).knownCombinations;

            const GameOperation &operationMessage = std::forward<GameOperation>(e);

            // update the state with the current players safe combination knowledge
            auto activeCharacter = state.getCharacters().getByUUID(fsm.activeCharacter);

            // Find player owning character
            std::optional<Player> player = std::nullopt;
            if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER1) {
                player = Player::one;
                spdlog::info("Character belongs to player one");
            } else if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER2) {
                player = Player::two;
                spdlog::info("Character belongs to player two");
            } else {
                spdlog::info("Character is NPC");
            }

            if (player.has_value()) {
                spdlog::info("Resetting strike count for player {} who had {} strikes.",
                             player.value(),
                             root_machine(fsm).strikeCounts[player.value()]);
                root_machine(fsm).strikeCounts[player.value()] = 0;
                state.setKnownSafeCombinations(knownCombinations.at(player.value()));
            }

            executeOperation(operationMessage.getOperation(),
                             state,
                             root_machine(fsm).matchConfig,
                             fsm.operations,
                             fsm.remainingCharacters);

            // if the player is hasn't got enough MP to leave the fog, his turn ends although there are AP left
            if (!Util::hasMPInFog(*activeCharacter, state)) {
                spdlog::info("Character {} is stuck in fog, thus AP are resetted.",
                             activeCharacter->getName());
                activeCharacter->setActionPoints(0);
            }

            if (player.has_value()) {
                // copy the potentially changed known combinations back to the map
                knownCombinations[player.value()] = state.getMySafeCombinations();
            }
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
            spdlog::info("Generating NPC action for {}", fsm.activeCharacter);

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
        void operator()(const Event &event, FSM &fsm, SourceState &, TargetState &target) {
            spdlog::info("RequestNextOperation: last active character was {}", fsm.activeCharacter);
            spy::gameplay::State &state = root_machine(fsm).gameState;

            if (spy::util::RoundUtils::isGameOver(state)) {
                // There may still be characters remaining, but the game has been won with the last action.
                // We do not have to request a new operation, and abort early.
                spdlog::info("Skipping requestNextOperation because game is already over.");
                root_machine(fsm).process_event(events::triggerGameEnd{});
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

            // Check if the next character has to be chosen or not
            bool advanceCharacter = true;
            if (fsm.activeCharacter != spy::util::UUID{} and
                state.getCharacters().findByUUID(fsm.activeCharacter) != state.getCharacters().end()) {
                // Some character was already active this round
                spdlog::debug("Last character was a regular character that might make another action");

                const spy::character::Character &activeCharacter = *state.getCharacters().findByUUID(
                        fsm.activeCharacter);

                if (not isRetire and Util::hasAPMP(activeCharacter)) {
                    spdlog::info("Character {} has not retired and can make another move.",
                                 activeCharacter.getName());
                    advanceCharacter = false;
                }
            }

            if (advanceCharacter) {
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
            } else {
                spdlog::info("Not choosing a new character.");
            }

            // Check if special handling for Cat and Janitor is required
            // Those already advance activeCharacter in their action, so we dont need to advance
            spdlog::info("Checking if activeCharacter({}) is cat ({})", fsm.activeCharacter, root_machine(fsm).catId);
            spdlog::info("Checking if activeCharacter({}) is janitor ({})", fsm.activeCharacter,
                         root_machine(fsm).janitorId);
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


            auto nextCharacter = state.getCharacters().getByUUID(fsm.activeCharacter);
            spdlog::info("Requesting operation from {}", nextCharacter->getName());

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

            if (not activePlayer.has_value()) {
                spdlog::debug("requestNextOperation determined that next character is not a PC"
                              "-> Not requesting, triggering NPC move instead.");
                root_machine(fsm).process_event(events::triggerNPCmove{});
                return;
            }

            spy::network::messages::RequestGameOperation request{
                    root_machine(fsm).playerIds.find(activePlayer.value())->second,
                    fsm.activeCharacter
            };
            MessageRouter &router = root_machine(fsm).router;
            spdlog::info("Requesting Operation from player {}", activePlayer.value());
            router.sendMessage(request);

            const spy::MatchConfig &matchConfig = root_machine(fsm).matchConfig;
            if (matchConfig.getTurnPhaseLimit().has_value()) {
                int turnPhaseLimitSeconds = matchConfig.getTurnPhaseLimit().value();
                spdlog::info("Starting turn phase timer for {} seconds", turnPhaseLimitSeconds);
                target.turnPhaseTimer.restart(std::chrono::seconds{turnPhaseLimitSeconds}, [
                        &fsm = root_machine(fsm),
                        player = *root_machine(fsm).playerIds.find(activePlayer.value()),
                        character = fsm.activeCharacter,
                        strikeMax = matchConfig.getStrikeMaximum()]() {
                    spdlog::warn("Turn phase time limit reached for player {}.", player.first);
                    fsm.strikeCounts[player.first]++;
                    spy::network::messages::Strike strikeMessage{
                            player.second,
                            fsm.strikeCounts[player.first],
                            static_cast<int>(strikeMax),
                            "Turn phase time limit reached."};
                    spdlog::info("Sending strike to player {}.", player.first);
                    fsm.router.sendMessage(std::move(strikeMessage));
                    // TODO skip turn without retire
                    spdlog::info("Executing retire.");
                    auto retireAction = std::make_shared<spy::gameplay::RetireAction>(character);
                    spy::network::messages::GameOperation retireOp{player.second, retireAction};
                    fsm.process_event(std::move(retireOp));
                });
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

            auto res = ActionExecutor::executeCat(state, *std::dynamic_pointer_cast<const CatAction>(catAction));
            fsm.operations.push_back(res);
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
            using spy::util::GameLogicUtils;

            State &state = root_machine(fsm).gameState;

            auto janitorAction = ActionGenerator::generateJanitorAction(state);
            auto janitorTarget = GameLogicUtils::getInCharacterSetByCoordinates(state.getCharacters(),
                                                                                janitorAction->getTarget());

            auto res = ActionExecutor::executeJanitor(state,
                                                      *std::dynamic_pointer_cast<const JanitorAction>(janitorAction));

            spdlog::debug("Janitor removes {}", janitorTarget->getName());

            fsm.operations.push_back(res);

            // remove character from the list of remaining characters
            auto it = std::find(fsm.remainingCharacters.begin(), fsm.remainingCharacters.end(),
                                janitorTarget->getCharacterId());
            if (it != fsm.remainingCharacters.end()) {
                fsm.remainingCharacters.erase(it);
                janitorTarget->setActionPoints(0);
                janitorTarget->setMovePoints(0);
            }
        }
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
