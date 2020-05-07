/**
 * @file   ChoiceHandling.hpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  FSM actions related to handling and requesting item choice operations
 */

#ifndef SERVER017_CHOICE_HANDLING_HPP
#define SERVER017_CHOICE_HANDLING_HPP

namespace actions {
    /**
     * @brief Action to apply if a choice is received.
     */
    struct handleChoice {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &s, TargetState &) {
            spdlog::info("Handling client choice");
            auto clientId = e.getclientId();
            auto choice = e.getChoice();
            auto &selection = s.selections.at(clientId);

            if (std::holds_alternative<spy::util::UUID>(choice)) {
                // client has chosen a character --> at it to chosen list and remove it from selection
                s.characterChoices.at(clientId).push_back(std::get<spy::util::UUID>(choice));
                selection.first.erase(std::remove(selection.first.begin(), selection.first.end(),
                                                  std::get<spy::util::UUID>(choice)), selection.first.end());
            } else {
                // client has chosen a gadget --> at it to chosen list and remove it from selection
                s.gadgetChoices.at(clientId).push_back(std::get<spy::gadget::GadgetEnum>(choice));
                selection.second.erase(std::remove(selection.second.begin(), selection.second.end(),
                                                   std::get<spy::gadget::GadgetEnum>(choice)), selection.second.end());
            }
            // add other possible selection items to the choice set and clear the selection for this client
            root_machine(fsm).choiceSet.addForSelection(selection.first, selection.second);
            selection.first.clear();
            selection.second.clear();
        }
    };

    /**
     * Request a new choice from the client.
     */
    struct requestNextChoice {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &s, TargetState &) {
            spdlog::debug("Check which client needs a choice request next");
            for (auto it = s.selections.begin(); it != s.selections.end(); it++) {
                if (it->second.first.size() == 0 &&
                    (s.characterChoices.at(it->first).size() + s.gadgetChoices.at(it->first).size() < 8)
                    && root_machine(fsm).choiceSet.isSelectionPossible()) {
                    // client still needs selections and selection is currently possible
                    it->second = root_machine(fsm).choiceSet.requestSelection();
                    spy::network::messages::RequestItemChoice message(it->first, it->second.first, it->second.second);
                    spdlog::info("Sending requestItemChoice to client {}", it->first);
                    MessageRouter &router = root_machine(fsm).router;
                    router.sendMessage(message);
                }
            }
        }
    };

    /**
     * @brief calls both actions::handleChoice and actions::requestNextChoice with the same parameters
     */
    struct handleChoiceAndRequestNext {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &source, TargetState &target) {
            handleChoice{}(std::forward<Event>(event), fsm, source, target);

            if (!guards::noChoiceMissing{}(fsm, source, event)) {
                requestNextChoice{}(std::forward<Event>(event), fsm, source, target);
            } else {
                root_machine(fsm).process_event(events::choicePhaseFinished{});
            }
        }
    };

    /**
     * @brief intended to create the final character set after the choices of the
     */
    struct createCharacterSet {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&/*event*/, FSM &/*fsm*/, SourceState &/*source*/, TargetState &/*target*/) {
            spdlog::info("create character Set");
        }
    };
}

#endif //SERVER017_CHOICE_HANDLING_HPP
