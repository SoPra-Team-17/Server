//
// Created by jonas on 04.05.20.
//

#ifndef SERVER017_SERVERUTIL_HPP
#define SERVER017_SERVERUTIL_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameOperation.hpp>
#include <network/messages/RequestGameOperation.hpp>
#include <util/Player.hpp>
#include "Util.hpp"

namespace guards {
    struct operationValid {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &, Event const &event) {
            nlohmann::json operationType = event.getType();
            spdlog::info("Checking GameOperation of type " + operationType.dump() + " ... valid");
            // TODO validate Operation
            return true;
        }
    };

    struct lastCharacter {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &) {
            return fsm.remainingCharacters.size() == 1;
        }
    };
}


namespace actions {
    struct handleOperation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &, SourceState &, TargetState &) {
            Util::handleOperationFunc(event);
        }
    };

    struct handleOperationAndRequestNext {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            Util::handleOperationFunc(event);
            // TODO: this could be in the transition action
            //  maybe proper guards so the transitions
            //                       operation
            //  waitingForOperation ----------------> waitingForOperation
            //                       /chooseNext
            //  and
            //                       last operation
            //  waitingForOperation ----------------> roundInit
            //  are both possible


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
}

#endif //SERVER017_SERVERUTIL_HPP
