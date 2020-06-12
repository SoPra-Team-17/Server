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
#include <Actions.hpp>

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
        spy::gadget::GadgetEnum::ANTI_PLAGUE_MASK
};

struct ChoicePhase : afsm::def::state_def<ChoicePhase> {
    using OfferMap = std::map<spy::util::UUID, Offer>;
    using CharacterMap = std::map<spy::util::UUID, std::vector<spy::util::UUID>>;
    using GadgetMap    = std::map<spy::util::UUID, std::vector<spy::gadget::GadgetEnum>>;
    using ChoiceCountMap = std::map<spy::util::UUID, unsigned int>;

    CharacterMap characterChoices;
    GadgetMap gadgetChoices;
    ChoiceCountMap choiceCount;

    OfferMap offers;

    template<typename FSM, typename Event>
    void on_enter(Event &&, FSM &fsm) {
        spdlog::info("Entering choice phase");

        // get access to the members of the root fsm
        const std::vector<spy::character::CharacterInformation> &characterInformations =
                root_machine(fsm).characterInformations;
        ChoiceSet &choiceSet = root_machine(fsm).choiceSet;
        MessageRouter &router = root_machine(fsm).router;
        const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;

        choiceSet.clear();
        choiceSet.addForSelection(characterInformations, possibleGadgets);

        auto idP1 = playerIds.at(Player::one);
        auto idP2 = playerIds.at(Player::two);

        characterChoices[idP1] = std::vector<spy::util::UUID>();
        characterChoices[idP2] = std::vector<spy::util::UUID>();

        gadgetChoices[idP1] = std::vector<spy::gadget::GadgetEnum>();
        gadgetChoices[idP2] = std::vector<spy::gadget::GadgetEnum>();

        choiceCount[idP1] = 0;
        choiceCount[idP2] = 0;

        offers[idP1] = choiceSet.requestSelection();
        offers[idP2] = choiceSet.requestSelection();

        auto offerP1 = offers.at(idP1);
        auto offerP2 = offers.at(idP2);

        spy::network::messages::RequestItemChoice messageP1(idP1, offerP1.characters, offerP1.gadgets);
        spy::network::messages::RequestItemChoice messageP2(idP2, offerP2.characters, offerP2.gadgets);

        router.sendMessage(messageP1);
        spdlog::info("Sending choice offer to player1 ({})", idP1);
        router.sendMessage(messageP2);
        spdlog::info("Sending choice offer to player2 ({})", idP2);
    }

    // @formatter:off
    using internal_transitions = transition_table <
    //  Event                              Action                                                                                                                                                                               Guard
    in<spy::network::messages::ItemChoice, actions::multiple<actions::handleChoice, actions::requestNextChoice>, and_<not_<guards::lastChoice>,                                                                                 guards::choiceValid>>,
    in<spy::network::messages::ItemChoice, actions::multiple<actions::replyWithError<spy::network::ErrorTypeEnum::ILLEGAL_MESSAGE>, actions::closeConnectionToClient, actions::broadcastGameLeft, actions::emitForceGameClose>, not_<guards::choiceValid>>
    >;
    // @formatter:on
};

#endif //SERVER017_CHOICEPHASEFSM_HPP
