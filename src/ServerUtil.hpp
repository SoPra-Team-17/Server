//
// Created by jonas on 04.05.20.
//

#ifndef SERVER017_SERVERUTIL_HPP
#define SERVER017_SERVERUTIL_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameOperation.hpp>
#include <network/messages/RequestGameOperation.hpp>
#include <util/Player.hpp>

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

    struct noCharactersRemaining {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &) {
            spdlog::info("Checking guard noCharactersRemaining: {} remaining characters", fsm.remainingCharacters.size());
            return fsm.remainingCharacters.size() == 0;
        }
    };
}


#endif //SERVER017_SERVERUTIL_HPP
