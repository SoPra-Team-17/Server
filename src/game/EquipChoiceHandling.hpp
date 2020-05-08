/**
 * @file   ChoiceHandling.hpp
 * @author Dominik Authaler
 * @date   07.05.2020 (creation)
 * @brief  FSM actions related to handling and requesting equipment choice messages.
 */

#ifndef SERVER017_EQUIP_CHOICE_HANDLING_HPP
#define SERVER017_EQUIP_CHOICE_HANDLING_HPP

#include <spdlog/spdlog.h>
#include "datatypes/gadgets/WiretapWithEarplugs.hpp"
#include "datatypes/gadgets/Gadget.hpp"

namespace actions {
    /**
     * @brief Action to apply if an equipment choice event was received.
     */
    struct handleEquipmentChoice {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &s, TargetState &) {
            auto clientId = e.getClientId();

            spy::character::CharacterSet &charSet = root_machine(fsm).gameState.getCharacters();

            auto equipChoice = e.getEquipment();

            if (clientId == root_machine(fsm).playerIds.at(Player::one)) {
                spdlog::info("Handling equipment choice of player one ({})", clientId);
            } else {
                spdlog::info("Handling equipment choice of player two ({})", clientId);
            }

            // List of all the players characters, only to print all of them (including those without gadget) to log
            auto chosenCharacters = s.chosenCharacters.at(clientId);

            for (const auto &[characterId, gadgetSet] : equipChoice) {
                auto character = charSet.getByUUID(characterId);
                spdlog::info("Character: {} ({})", character->getName(), characterId);

                for (const auto &g : gadgetSet) {
                    spdlog::info("\t {}", fmt::json(g));
                    if (g == spy::gadget::GadgetEnum::WIRETAP_WITH_EARPLUGS) {
                        character->addGadget(std::make_shared<spy::gadget::WiretapWithEarplugs>());
                    } else {
                        character->addGadget(std::make_shared<spy::gadget::Gadget>(g));
                    }
                }
                // remove the character id from the copy of the list of chosen characters to determine which ones
                // have no gadgets equipped
                chosenCharacters.erase(std::remove(chosenCharacters.begin(), chosenCharacters.end(),
                                                                            characterId), chosenCharacters.end());
            }

            // Print message to log for characters not explicitly mentioned in equipment choice
            for (const auto &characterId : chosenCharacters) {
                auto character = charSet.findByUUID(characterId);
                spdlog::info("Character: {} ({})", character->getName(), characterId);
            }

            s.hasChosen.at(clientId) = true;
        }
    };
}

#endif //SERVER017_EQUIP_CHOICE_HANDLING_HPP
