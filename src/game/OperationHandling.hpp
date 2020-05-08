/**
 * @file OperationHandling.hpp
 * @brief FSM actions relating to GameOperation requesting and handling
 * @author ottojo
 */

#ifndef SERVER017_OPERATIONHANDLING_HPP
#define SERVER017_OPERATIONHANDLING_HPP


#include <spdlog/spdlog.h>
#include <util/Player.hpp>
#include <network/messages/GameStatus.hpp>
#include <gameLogic/execution/ActionExecutor.hpp>
#include <util/RoundUtils.hpp>
#include <util/Format.hpp>

namespace actions {
    /**
     * @brief Action to apply a valid operation to the state
     */
    struct handleOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &, TargetState &) {
            using spy::gameplay::ActionExecutor;
            using spy::network::messages::GameOperation;
            using spy::util::RoundUtils;
            spdlog::info("Handling some operation");

            const GameOperation &operationMessage = std::forward<GameOperation>(e);
            spy::gameplay::State &state = root_machine(fsm).gameState;
            auto operation = operationMessage.getOperation();


            spdlog::info("Executing {} action", fmt::json(operation->getType()));
            bool operationSuccessful = ActionExecutor::execute(state, operation, root_machine(fsm).matchConfig);

            auto operationWithResult = operation->clone();
            operationWithResult->setSuccessful(operationSuccessful);
            fsm.operations.push_back(operationWithResult);

            // TODO: execute potentially resulting exfiltration, add those to fsm.operations

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
            spy::gameplay::State &s = root_machine(fsm).gameState;
            MessageRouter &router = root_machine(fsm).router;

            spy::network::messages::GameStatus g{{}, fsm.activeCharacter, fsm.operations, s, false};
            router.broadcastMessage(g);
            fsm.operations.clear();
        }
    };

    /**
     * Request operation from next character in list
     */
    struct requestNextOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            spdlog::trace("requestNextOperation for character {}", fsm.activeCharacter);
            spy::gameplay::State &state = root_machine(fsm).gameState;

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
                    spdlog::warn("activeCharacter ({}) does not have valid faction, NPCs not yet implemented",
                                 fsm.activeCharacter);
                    return;
            }

            spy::network::messages::RequestGameOperation request{
                    root_machine(fsm).playerIds.find(activePlayer)->second,
                    fsm.activeCharacter
            };
            MessageRouter &router = root_machine(fsm).router;
            spdlog::info("Requesting Operation for character {} from player ", activeChar->getName(), activePlayer);
            router.sendMessage(request);
        }
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
