/**
 * @file   ChoicePhaseFSM.hpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  State machine for the choice phase.
 */

#ifndef SERVER017_CHOICEPHASEFSM_HPP
#define SERVER017_CHOICEPHASEFSM_HPP

#include <afsm/fsm.hpp>
#include <network/messages/RequestItemChoice.hpp>
#include <map>

#include "game/ChoiceHandling.hpp"

static const std::vector<spy::gadget::GadgetEnum> possibleGadgets = {
        spy::gadget::GadgetEnum::HAIRDRYER,
        spy::gadget::GadgetEnum::MOLEDIE,
        spy::gadget::GadgetEnum::TECHNICOLOUR_PRISM,
        spy::gadget::GadgetEnum::BOWLER_BLADE,
        spy::gadget::GadgetEnum::MAGNETIC_WATCH,
        spy::gadget::GadgetEnum::POISON_PILLS,
        spy::gadget::GadgetEnum::LASER_COMPACT,
        spy::gadget::GadgetEnum::ROCKET_PEN,
        spy::gadget::GadgetEnum::GAS_GLOSS,
        spy::gadget::GadgetEnum::MOTHBALL_POUCH,
        spy::gadget::GadgetEnum::FOG_TIN,
        spy::gadget::GadgetEnum::GRAPPLE,
        spy::gadget::GadgetEnum::WIRETAP_WITH_EARPLUGS,
        spy::gadget::GadgetEnum::JETPACK,
        spy::gadget::GadgetEnum::CHICKEN_FEED,
        spy::gadget::GadgetEnum::NUGGET,
        spy::gadget::GadgetEnum::MIRROR_OF_WILDERNESS,
        spy::gadget::GadgetEnum::POCKET_LITTER,
};

struct ChoicePhase : afsm::def::state_def<ChoicePhase> {
    std::map<spy::util::UUID, std::vector<spy::util::UUID>> characterChoices;
    std::map<spy::util::UUID, std::vector<spy::gadget::GadgetEnum>> gadgetChoices;

    std::map<spy::util::UUID, std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>>> selections;

    template<typename FSM, typename Event>
    void on_enter(Event &&, FSM &fsm) {
        spdlog::info("Entering choice phase");
                                                                    // get access to the members of the root fsm
        std::vector<spy::character::CharacterInformation> &characterInformations =
                root_machine(fsm).characterInformations;
        ChoiceSet &choiceSet = root_machine(fsm).choiceSet;
        //MessageRouter &router = root_machine(fsm).router;

        choiceSet.addForSelection(characterInformations, possibleGadgets);

        //selections.at(playerID1) = choiceSet.requestSelection();
        //spy::network::messages::RequestItemChoice messageP1(spy::util::UUID(), selectionP1.first, selectionP1.second);
        //router.sendMessage()

        // depending on the size of the choice set a parallel selection is not always possible
        if (choiceSet.isSelectionPossible()) {
            //selections.at(playerID2) = choiceSet.requestSelection();
            //spy::network::messages::RequestItemChoice messageP2(spy::util::UUID(), selectionP1.first, selectionP1.second);
            //router.sendMessage()
        }
    }

    // @formatter:off
    using internal_transitions = transition_table <
    //  Event                                 Action                            Guard
    in<spy::network::messages::ItemChoice, actions::handleChoiceAndRequestNext, not_ < guards::noChoiceMissing>>
    >;
    // @formatter:on
};

#endif //SERVER017_CHOICEPHASEFSM_HPP
