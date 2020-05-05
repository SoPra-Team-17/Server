//
// Created by jonas on 05.05.20.
//

#ifndef SERVER017_OPERATIONHANDLING_HPP
#define SERVER017_OPERATIONHANDLING_HPP


#include <spdlog/spdlog.h>
#include <util/Player.hpp>

class OperationHandling {

};


namespace actions {
    struct handleOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &, SourceState &, TargetState &) {
            spdlog::info("Handling some operation");
        }
    };

    /**
     * Request operation from next character in list
     */
    struct requestNextOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {

            fsm.activeCharacter = fsm.remainingCharacters.front();
            fsm.remainingCharacters.pop_front();

            spdlog::info("Requesting GameOperation for character {}", fsm.activeCharacter);
            // TODO: find player owning activeCharacter
            auto activePlayer = Player::one;
            spdlog::info("Requesting GameOperation from player {}", activePlayer);
            // TODO properly request Operation
            //root_machine(fsm).router.sendMessage(static_cast<int>(activePlayer), spy::network::messages::RequestGameOperation());
        }
    };


    struct handleOperationAndRequestNext {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &source, TargetState &target) {
            // TODO find a way to directly specify multiple actions in transition table
            handleOperation{}(event, fsm, source, target);
            requestNextOperation{}(event, fsm, source, target);
        }
    };
}

#endif //SERVER017_OPERATIONHANDLING_HPP
