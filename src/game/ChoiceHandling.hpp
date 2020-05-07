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
            auto &offer = s.offers.at(clientId);

            if (std::holds_alternative<spy::util::UUID>(choice)) {
                // client has chosen a character --> add it to chosen list and remove it from selection
                s.characterChoices.at(clientId).push_back(std::get<spy::util::UUID>(choice));
                offer.characters.erase(std::remove(offer.characters.begin(), offer.characters.end(),
                                                  std::get<spy::util::UUID>(choice)), offer.characters.end());
            } else {
                // client has chosen a gadget --> add it to chosen list and remove it from selection
                s.gadgetChoices.at(clientId).push_back(std::get<spy::gadget::GadgetEnum>(choice));
                offer.gadgets.erase(std::remove(offer.gadgets.begin(), offer.gadgets.end(),
                                                   std::get<spy::gadget::GadgetEnum>(choice)), offer.gadgets.end());
            }
            // add other possible selection items to the choice set and clear the selection for this client
            root_machine(fsm).choiceSet.addForSelection(offer.characters, offer.gadgets);
            offer.characters.clear();
            offer.gadgets.clear();
        }
    };

    /**
     * Request a new choice from the client.
     */
    struct requestNextChoice {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &s, TargetState &) {
            spdlog::debug("Check which client needs a choice request next");

            for (auto& [playerId, offer] : s.offers) {
                if (!root_machine(fsm).choiceSet.isOfferPossible()) {
                    return;
                }

                bool hasNoOffer = (offer.characters.size() == 0);
                bool choicesMissing = (s.characterChoices.at(playerId).size() + s.gadgetChoices.at(playerId).size() < 8);

                if (hasNoOffer && choicesMissing) {
                    // client still needs selections and selection is currently possible
                    offer = root_machine(fsm).choiceSet.requestSelection();
                    spy::network::messages::RequestItemChoice message(playerId, offer.characters, offer.gadgets);
                    spdlog::info("Sending requestItemChoice to client {}", playerId);
                    MessageRouter &router = root_machine(fsm).router;
                    router.sendMessage(message);
                }
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
