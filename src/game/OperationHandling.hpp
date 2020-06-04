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

namespace actions {
    /**
     * @brief Action to apply a valid operation to the state and set next character as active
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
            Player player;
            if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER1) {
                player = Player::one;
            } else if (activeCharacter->getFaction() == spy::character::FactionEnum::PLAYER2) {
                player = Player::two;
            } else {
                spdlog::error("[HandleOperation] active character is no player character");
                return;
            }

            state.setKnownSafeCombinations(knownCombinations.at(player));


            executeOperation(operationMessage.getOperation(),
                             state,
                             root_machine(fsm).matchConfig,
                             fsm.operations);

            // copy the potentially changed known combinations back to the map
            knownCombinations[player] = state.getMySafeCombinations();

            // Choose next character
            if (!fsm.remainingCharacters.empty()) {
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();

                //check if character owns the anti plague mask and apply it's effect if necessary
                auto character = root_machine(fsm).gameState.getCharacters().getByUUID(fsm.activeCharacter);
                if (character->hasGadget(spy::gadget::GadgetEnum::ANTI_PLAGUE_MASK)) {
                    character->addHealthPoints(10);
                }
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
     * @brief Generates and executes a NPC action and sets next character as active
     */
    struct npcMove {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Generating NPC action");

            using spy::gameplay::ActionGenerator;
            auto npcAction = ActionGenerator::generateNPCAction(root_machine(fsm).gameState,
                                                                fsm.activeCharacter,
                                                                root_machine(fsm).matchConfig);

            if (npcAction != nullptr) {
                executeOperation(npcAction,
                                 root_machine(fsm).gameState,
                                 root_machine(fsm).matchConfig,
                                 fsm.operations);
            } else {
                spdlog::error("Generating NPC action failed.");
            }

            if (!fsm.remainingCharacters.empty()) {
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();
            }
        }
    };

    /**
     * Request operation from next character in list, emits events::triggerNPCmove if next is NPC
     */
    struct requestNextOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::trace("requestNextOperation for character {}", fsm.activeCharacter);
            spy::gameplay::State &state = root_machine(fsm).gameState;
            using spy::util::RoundUtils;

            if (RoundUtils::isGameOver(state)) {
                // There are still characters remaining, but the game has been won with the last action
                // We do not have to request a new operation, and abort early
                spdlog::info("Skipping requestNextOperation because game is already over.");
                return;
            }

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
            } else {
                // Find character object by UUID to determine faction
                auto activeChar = std::find_if(state.getCharacters().begin(),
                                               state.getCharacters().end(),
                                               [&fsm](const spy::character::Character &c) {
                                                   return c.getCharacterId() == fsm.activeCharacter;
                                               });

                Player activePlayer;
                switch (activeChar->getFaction()) {
                    case spy::character::FactionEnum::PLAYER1:
                        activePlayer = Player::one;
                        break;
                    case spy::character::FactionEnum::PLAYER2:
                        activePlayer = Player::two;
                        break;
                    default:
                        // Do not request when next is NPCmove
                        spdlog::debug("requestNextOperation determined that next character is not a PC"
                                      "-> Not requesting, triggering NPC move instead.");
                        root_machine(fsm).process_event(events::triggerNPCmove{});
                        return;
                }

                spy::network::messages::RequestGameOperation request{
                        root_machine(fsm).playerIds.find(activePlayer)->second,
                        fsm.activeCharacter
                };
                MessageRouter &router = root_machine(fsm).router;
                spdlog::info("Requesting Operation for character {} from player {}", activeChar->getName(), activePlayer);
                router.sendMessage(request);
            }
        }
    };

    /**
     * @brief Generates and executes a cat movement and sets next character as active
     */
    struct catMove {
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

            if (!fsm.remainingCharacters.empty()) {
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();
            }
        }
    };

    /**
     * @brief Generates and executes a janitor movement and sets next character as active
     */
    struct janitorMove {
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

            if (!fsm.remainingCharacters.empty()) {
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();
            }
        }
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
