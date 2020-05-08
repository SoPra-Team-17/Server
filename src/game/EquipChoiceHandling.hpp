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

            for (const auto &[characterId, gadgetSet] : equipChoice) {
                auto character = charSet.getByUUID(characterId);

                for (const auto &g : gadgetSet) {
                    if (g == spy::gadget::GadgetEnum::WIRETAP_WITH_EARPLUGS) {
                        character->addGadget(std::make_shared<spy::gadget::WiretapWithEarplugs>());
                    } else {
                        character->addGadget(std::make_shared<spy::gadget::Gadget>(g));
                    }
                }
            }

            s.chosenCharacters.at(clientId).clear();
            s.chosenGadgets.at(clientId).clear();
        }
    };
}

#endif //SERVER017_EQUIP_CHOICE_HANDLING_HPP
