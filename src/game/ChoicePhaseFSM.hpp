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
    using SelectionMap = std::map<spy::util::UUID, std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>>>;
    using CharacterMap = std::map<spy::util::UUID, std::vector<spy::util::UUID>>;
    using GadgetMap    = std::map<spy::util::UUID, std::vector<spy::gadget::GadgetEnum>>;

    CharacterMap characterChoices;
    GadgetMap gadgetChoices;

    SelectionMap selections;

    template<typename FSM, typename Event>
    void on_enter(Event &&, FSM &fsm) {
        spdlog::info("Entering choice phase");
                                                                    // get access to the members of the root fsm
        const std::vector<spy::character::CharacterInformation> &characterInformations =
                root_machine(fsm).characterInformations;
        ChoiceSet &choiceSet = root_machine(fsm).choiceSet;
        MessageRouter &router = root_machine(fsm).router;
        const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;

        choiceSet.addForSelection(characterInformations, possibleGadgets);

        auto idP1 = playerIds.at(Player::one);
        auto idP2 = playerIds.at(Player::two);

        selections[idP1] = choiceSet.requestSelection();
        selections[idP2] = choiceSet.requestSelection();
        auto selectionP1 = selections.at(idP1);
        auto selectionP2 = selections.at(idP2);

        spy::network::messages::RequestItemChoice messageP1(idP1, selectionP1.first, selectionP1.second);
        spy::network::messages::RequestItemChoice messageP2(idP2, selectionP2.first, selectionP2.second);

        router.sendMessage(messageP1);
        spdlog::info("Sending choice offer to {}", idP1);
        router.sendMessage(messageP2);
        spdlog::info("Sending choice offer to {}", idP2);
    }

    // @formatter:off
    using internal_transitions = transition_table <
    //  Event                                 Action                            Guard
    in<spy::network::messages::ItemChoice, actions::handleChoiceAndRequestNext, and_ < guards::choiceValid, not_ < guards::noChoiceMissing>>>
    >;
    // @formatter:on
};

#endif //SERVER017_CHOICEPHASEFSM_HPP
