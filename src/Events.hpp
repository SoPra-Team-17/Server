/**
 * @file Events.hpp
 * @author ottojo
 * @brief additional events for server state machine
 */

#ifndef SERVER017_EVENTS_HPP
#define SERVER017_EVENTS_HPP

#include <util/Player.hpp>
#include <datatypes/statistics/VictoryEnum.hpp>
#include <network/ErrorTypeEnum.hpp>

namespace events {
    struct triggerNPCmove {
    };

    struct roundInitDone {
    };

    struct forceUnpause {
    };

    struct triggerCatMove {
    };

    struct triggerJanitorMove {
    };

    struct roundDone {
    };

    struct playerDisconnect {
        spy::util::UUID clientId;
    };

    struct forceGameClose {
        Player winner;
        spy::statistics::VictoryEnum reason;
    };

    struct triggerGameEnd {
    };

    struct kickClient {
        spy::util::UUID clientId;
        std::optional<spy::network::ErrorTypeEnum> error = std::nullopt;
    };

    struct skipOperation {
    };
}
#endif //SERVER017_EVENTS_HPP
