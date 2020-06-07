/**
 * @file Events.hpp
 * @author ottojo
 * @brief additional events for server state machine
 */

#ifndef SERVER017_EVENTS_HPP
#define SERVER017_EVENTS_HPP

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
}
#endif //SERVER017_EVENTS_HPP
