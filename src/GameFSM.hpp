//
// Created by jonas on 04.05.20.
//

#ifndef SERVER017_GAMEFSM_HPP
#define SERVER017_GAMEFSM_HPP

#include <afsm/fsm.hpp>
#include <Events.hpp>

struct GameFSM : afsm::def::state_machine<GameFSM> {

    struct choicePhase : state<choicePhase> {
    };
    struct equipPhase : state<equipPhase> {
    };

    struct gamePhase : state_machine<gamePhase> {
        //Cocktails + Zugreihenfolge
        struct waitingForOperation : state<waitingForOperation> {
        };
        struct roundInit : state<roundInit> {
        };

        using initial_state = roundInit;

        using transitions = transition_table <
        // Start        Event                        Next
        tr<roundInit,   events::roundInitDone,       waitingForOperation>
        >;
    };

    struct gameOver : state<gameOver> {
    };

    using initial_state = choicePhase;

    template<typename FSM, typename Event>
    void on_enter(Event &&, FSM &) {
        spdlog::info("Entering Game State");
    }

    using transitions = transition_table <
    // Start        Event                        Next
    tr<choicePhase, events::choicePhaseFinished, equipPhase>,
    tr<equipPhase,  events::equipPhaseFinished,  gamePhase>,
    tr<gamePhase,   events::gameFinished,        gameOver>
    >;
};

#endif //SERVER017_GAMEFSM_HPP
