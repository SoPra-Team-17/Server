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
            spdlog::info("Handling some operation");

            const GameOperation &operationMessage = std::forward<GameOperation>(e);

            executeOperation(operationMessage.getOperation(),
                             root_machine(fsm).gameState,
                             root_machine(fsm).matchConfig,
                             fsm.operations);

            // Choose next character
            if (!fsm.remainingCharacters.empty()) {
                fsm.activeCharacter = fsm.remainingCharacters.front();
                fsm.remainingCharacters.pop_front();
            }
        }
    };

    struct broadcastState {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::info("Broadcasting state");
            spy::gameplay::State &state = root_machine(fsm).gameState;
            MessageRouter &router = root_machine(fsm).router;

            spy::network::messages::GameStatus g{
                    {}, // clientId filled out by router when broadcasting
                    fsm.activeCharacter,
                    fsm.operations,
                    state,
                    root_machine(fsm).isGameOver};
            router.broadcastMessage(g);
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
            auto npcAction = ActionGenerator::generateNPCAction(root_machine(fsm).gameState, fsm.activeCharacter);

            if (npcAction != nullptr) {
                root_machine(fsm).isGameOver = executeOperation(npcAction,
                                                                root_machine(fsm).gameState,
                                                                root_machine(fsm).matchConfig,
                                                                fsm.operations);
            } else {
                spdlog::error("Generating NPC action failed.");
                spdlog::warn("Pretending this NPC action won the game");
                root_machine(fsm).isGameOver = true;
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
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
