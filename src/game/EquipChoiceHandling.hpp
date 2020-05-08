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
            spdlog::info("Handling equipment choice");
            auto clientId = e.getClientId();

            spy::character::CharacterSet &charSet = root_machine(fsm).gameState.getCharacters();

            auto equipChoice = e.getEquipment();

            if (clientId == root_machine(fsm).playerIds.at(Player::one)) {
                spdlog::info("Equipment choice of player one ({})", clientId);
            } else {
                spdlog::info("Equipment choice of player two ({})", clientId);
            }

            auto &chosenCharacters = s.chosenCharacters.at(clientId);

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
                chosenCharacters.erase(std::remove(chosenCharacters.begin(), chosenCharacters.end(),
                                                                            characterId), chosenCharacters.end());
            }

            for (const auto &characterId : chosenCharacters) {
                auto character = charSet.findByUUID(characterId);
                spdlog::info("Character: {} ({})", character->getName(), characterId);
            }

            s.chosenCharacters.at(clientId).clear();
            s.chosenGadgets.at(clientId).clear();
        }
    };
}

#endif //SERVER017_EQUIP_CHOICE_HANDLING_HPP
