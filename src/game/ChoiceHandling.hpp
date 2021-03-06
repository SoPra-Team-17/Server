/**
 * @file   ChoiceHandling.hpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  FSM actions related to handling and requesting item choice operations
 */

#ifndef SERVER017_CHOICE_HANDLING_HPP
#define SERVER017_CHOICE_HANDLING_HPP

#include <datatypes/gadgets/WiretapWithEarplugs.hpp>

namespace actions {
    constexpr int maxNumberOfCharacters = 4;
    constexpr int maxNumberOfGadgets = 6;
    constexpr int requiredNumberOfChoices = 8;

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

            for (auto &[playerId, offer]: s.offers) {
                bool hasNoOffer = (offer.characters.empty() && offer.gadgets.empty());
                bool choicesMissing = ((s.characterChoices.at(playerId).size() + s.gadgetChoices.at(playerId).size())
                                       < requiredNumberOfChoices);

                if (hasNoOffer && choicesMissing) {
                    // client still needs selections and selection is currently possible
                    if (s.characterChoices.at(playerId).size() >= maxNumberOfCharacters &&
                        root_machine(fsm).choiceSet.isGadgetOfferPossible()) {
                        // client has chosen maximum number of characters, thus offer only gadgets
                        offer = root_machine(fsm).choiceSet.requestGadgetSelection();
                    } else if (s.gadgetChoices.at(playerId).size() >= maxNumberOfGadgets &&
                               root_machine(fsm).choiceSet.isCharacterOfferPossible()) {
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
     * Sends the current offer to the reconnecting client
     */
    struct repeatChoiceOffer {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(const Event &event, FSM &fsm, SourceState &, const TargetState &target) {
            const spy::network::messages::Reconnect &reconnect = event;

            auto offer = target.offers.find(reconnect.getClientId());
            if (offer == target.offers.end()) {
                spdlog::error("Reconnect of client {}, no offer found. Closing game.", reconnect.getClientId());
                root_machine(fsm).process_event(
                        events::forceGameClose{
                                Player::one,
                                spy::statistics::VictoryEnum::VICTORY_BY_RANDOMNESS});
                return;
            }
            spy::network::messages::RequestItemChoice message(reconnect.getClientId(), offer->second.characters,
                                                              offer->second.gadgets);

            spdlog::info("Repeating choice offer for player {} after reconnect.", reconnect.getClientId());
            root_machine(fsm).router.sendMessage(message);
        }
    };

    /**
     * Creates the final character set after the choices of the clients.
     */
    struct createCharacterSet {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &s, TargetState &t) {
            spdlog::info("adding chosen characters to the character set");

            const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
            const std::vector<spy::character::CharacterInformation> &charInfos =
                    root_machine(fsm).characterInformations;
            spy::gameplay::State &gameState = root_machine(fsm).gameState;
            const ChoiceSet &choiceSet = root_machine(fsm).choiceSet;
            const unsigned int maxNumberOfNPCs = root_machine(fsm).maxNumberOfNPCs;

            auto charsP1 = s.characterChoices.at(playerIds.at(Player::one));
            auto charsP2 = s.characterChoices.at(playerIds.at(Player::two));

            spy::character::CharacterSet charSet;

            spy::character::FactionEnum faction;

            auto remainingCharacters = choiceSet.getRemainingCharacters();
            auto remainingGadgets = choiceSet.getRemainingGadgets();

            std::list<spy::util::UUID> npcCharacters;
            // choose characters that will be NPCs
            if (remainingCharacters.size() <= maxNumberOfNPCs) {
                npcCharacters = remainingCharacters;
            } else {
                for (auto i = 0U; i < maxNumberOfNPCs; i++) {
                    auto uuidIterator = spy::util::GameLogicUtils::getRandomItemFromContainer(remainingCharacters);
                    npcCharacters.push_back(*uuidIterator);
                    remainingCharacters.erase(uuidIterator);
                }
            }

            // for each character, insert it into the character set if it has been chosen by a client or
            // assigned to the NPC faction
            for (const auto &c : charInfos) {
                // search for the character UUID in the list of characters of the players and the NPCs and
                // set it's faction to the corresponding value
                if (std::find(charsP1.begin(), charsP1.end(), c.getCharacterId()) != charsP1.end()) {
                    faction = spy::character::FactionEnum::PLAYER1;
                } else if (std::find(charsP2.begin(), charsP2.end(), c.getCharacterId()) != charsP2.end()) {
                    faction = spy::character::FactionEnum::PLAYER2;
                } else if (std::find(npcCharacters.begin(), npcCharacters.end(), c.getCharacterId()) !=
                           npcCharacters.end()) {
                    faction = spy::character::FactionEnum::NEUTRAL;
                } else {
                    continue;
                }

                auto character = spy::character::Character{c.getCharacterId(), c.getName()};
                character.setProperties(
                        std::set<spy::character::PropertyEnum>{c.getFeatures().begin(), c.getFeatures().end()});
                character.setFaction(faction);
                charSet.insert(character);
            }

            // distribute remaining gadgets to NPCs
            for (const auto &g : remainingGadgets) {
                auto uuidIterator = spy::util::GameLogicUtils::getRandomItemFromContainer(npcCharacters);
                if (g == spy::gadget::GadgetEnum::WIRETAP_WITH_EARPLUGS) {
                    charSet.getByUUID(*uuidIterator)->addGadget(std::make_shared<spy::gadget::WiretapWithEarplugs>());
                } else if (g != spy::gadget::GadgetEnum::NUGGET
                           && g != spy::gadget::GadgetEnum::MIRROR_OF_WILDERNESS
                           && g != spy::gadget::GadgetEnum::CHICKEN_FEED) {
                    charSet.getByUUID(*uuidIterator)->addGadget(std::make_shared<spy::gadget::Gadget>(g));
                }
            }

            // log the initial gadgets of the NPCs
            spdlog::info("NPCs:");
            for (const auto &characterId : npcCharacters) {
                auto character = charSet.findByUUID(characterId);
                spdlog::info("Character: {} ({})", character->getName(), characterId);
                for (const auto &g : character->getGadgets()) {
                    spdlog::info("\t {}", fmt::json(g->getType()));
                }
            }

            gameState.setCharacters(charSet);

            // give information about choices to the equip phase
            t.chosenCharacters = s.characterChoices;
            t.chosenGadgets = s.gadgetChoices;
        }
    };

    struct startChoicePhaseTimer {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(const Event &event, FSM &fsm, SourceState &, TargetState &target) {
            std::optional<unsigned int> reconnectLimit = root_machine(fsm).matchConfig.getReconnectLimit();
            if (reconnectLimit.has_value()) {
                spy::util::UUID playerId;
                if constexpr(std::is_same<Event, events::playerDisconnect>::value) {
                    playerId = event.clientId;
                } else {
                    playerId = event.getClientId();
                }

                auto playerOneId = root_machine(fsm).playerIds.find(Player::one);
                if (playerOneId != root_machine(fsm).playerIds.end()
                    and playerOneId->second == playerId) {
                    spdlog::info("Starting choice phase reconnect timer for player one for {} seconds",
                                 reconnectLimit.value());
                    target.playerOneReconnectTimer.restart(
                            std::chrono::seconds{reconnectLimit.value()},
                            [&fsm]() {
                                TargetState::limitReached(fsm, Player::one);
                            });
                } else {
                    spdlog::info("Starting choice phase reconnect timer for player two for {} seconds",
                                 reconnectLimit.value());
                    target.playerTwoReconnectTimer.restart(
                            std::chrono::seconds{reconnectLimit.value()},
                            [&fsm]() {
                                TargetState::limitReached(fsm, Player::two);
                            });
                }
            }
        }
    };
}

#endif //SERVER017_CHOICE_HANDLING_HPP
