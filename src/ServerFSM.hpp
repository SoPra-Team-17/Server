/**
 * @file   Server.hpp
 * @author Dominik Authaler
 * @date   03.05.2020 (creation)
 * @brief  Declaration of the main finite state machine for the server.
 */

#ifndef SERVER017_SERVERFSM_HPP
#define SERVER017_SERVERFSM_HPP

#include <afsm/fsm.hpp>

namespace events {
    struct initDone{};
    struct killServer{};
    struct playerEntered {};
    struct playerLeft{};
}

struct ServerSM : afsm::def::state_machine<ServerSM> {
    // States
    struct initial : state<initial> {};

    struct running : state_machine<running> {
        struct emptyLobby : state<emptyLobby> {};
        struct waitForPlayers : state<waitForPlayers>{};

        using initial_state = emptyLobby;

        using transitions = transition_table<
            // Start        Event                 Next
            tr<emptyLobby, events::playerEntered, waitForPlayers>
        >;
    };

    struct shutdown : state<shutdown> {};

    using initial_state = initial;

    // State transitions
    using transitions = transition_table<
        // Start    Event             Next
        tr<initial, events::initDone, running>,
        tr<running, events::killServer, shutdown>
    >;
};

using ServerFSM = afsm::state_machine<ServerSM>;

#endif //SERVER017_SERVERFSM_HPP
