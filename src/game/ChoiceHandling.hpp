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
        void operator()(Event &e, FSM &fsm, SourceState &s, TargetState &) {
            auto clientId = e.getClientId();
            auto choice = e.getChoice();
            auto &offer = s.offers.at(clientId);

            s.choiceCount.at(clientId) += 1;

            if (clientId == root_machine(fsm).playerIds.at(Player::one)) {
                spdlog::info("Handling item choice from player 1 [{}/8]", s.choiceCount.at(clientId));
            } else {
                spdlog::info("Handling item choice from player 2 [{}/8]", s.choiceCount.at(clientId));
            }

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
                bool hasNoOffer = (offer.characters.empty() && offer.gadgets.empty());
                bool choicesMissing = ((s.characterChoices.at(playerId).size() + s.gadgetChoices.at(playerId).size()) < 8);

                if (hasNoOffer && choicesMissing) {
                    // client still needs selections and selection is currently possible
                    if (s.characterChoices.at(playerId).size() >= 4 && root_machine(fsm).choiceSet.isGadgetOfferPossible()) {
                        // client has chosen maximum number of characters, thus offer only gadgets
                        offer = root_machine(fsm).choiceSet.requestGadgetSelection();
                    } else if (s.gadgetChoices.at(playerId).size() >= 6 && root_machine(fsm).choiceSet.isCharacterOfferPossible()) {
                        // client has chosen maximum number of gadgets, thus offer only characters
                        offer = root_machine(fsm).choiceSet.requestCharacterSelection();
                    } else if (root_machine(fsm).choiceSet.isOfferPossible()) {
                        // client can choose both
                        offer = root_machine(fsm).choiceSet.requestSelection();
                    } else {
                        // no offer is possible
                        continue;
                    }

                    spy::network::messages::RequestItemChoice message(playerId, offer.characters, offer.gadgets);
                    MessageRouter &router = root_machine(fsm).router;
                    router.sendMessage(message);

                    if (playerId == root_machine(fsm).playerIds.at(Player::one)) {
                        spdlog::info("Sending new requestItemChoice to player 1 ({})", playerId);
                    } else {
                        spdlog::info("Sending new requestItemChoice to player 2 ({})", playerId);
                    }
                }
            }
        }
    };

    /**
     * @brief intended to create the final character set after the choices of the
     */
    struct createCharacterSet {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &s, TargetState &t) {
            spdlog::info("adding chosen characters to the character set");

            const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
            const std::vector<spy::character::CharacterInformation> &charInfos =
                    root_machine(fsm).characterInformations;
            spy::gameplay::State &gameState = root_machine(fsm).gameState;

            auto charsP1 = s.characterChoices.at(playerIds.at(Player::one));
            auto charsP2 = s.characterChoices.at(playerIds.at(Player::two));

            spy::character::CharacterSet charSet;

            spy::character::FactionEnum faction;
            //TODO: check if all non-chosen characters are NPCs
            for (const auto &c : charInfos) {
                if (std::find(charsP1.begin(), charsP1.end(), c.getCharacterId()) != charsP1.end()) {
                    faction = spy::character::FactionEnum::PLAYER1;
                } else if (std::find(charsP2.begin(), charsP2.end(), c.getCharacterId()) != charsP2.end()) {
                    faction = spy::character::FactionEnum::PLAYER2;
                } else {
                    // not chosen -> NPC
                    faction = spy::character::FactionEnum::NEUTRAL;
                }

                auto character = spy::character::Character{c.getCharacterId(), c.getName()};
                character.setProperties(
                        std::set<spy::character::PropertyEnum>{c.getFeatures().begin(), c.getFeatures().end()});
                character.setFaction(faction);
                charSet.insert(character);
            }

            gameState.setCharacters(charSet);

            // give information about choices to the equip phase
            t.chosenCharacters = s.characterChoices;
            t.chosenGadgets = s.gadgetChoices;
        }
    };
}

#endif //SERVER017_CHOICE_HANDLING_HPP
