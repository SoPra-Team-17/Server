//
// Created by jonas on 04.05.20.
//

#ifndef SERVER017_SERVERUTIL_HPP
#define SERVER017_SERVERUTIL_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameOperation.hpp>
#include <network/messages/RequestGameOperation.hpp>

namespace guards {
    struct operationValid {
        template<typename FSM, typename FSMState>
        bool operator()(FSM const &, FSMState const &, spy::network::messages::GameOperation const &event) {
            nlohmann::json operationType = event.getType();
            spdlog::info("Checking GameOperation of type " + operationType.dump() + " ... valid");
            // TODO validate Operation
            return true;
        }
    };
}

namespace actions {
    struct nextCharacter {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            // TODO set next character as currently active
        }
    };
}

#endif //SERVER017_SERVERUTIL_HPP
