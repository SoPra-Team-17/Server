/**
 * @file Guards.hpp
 * @brief FSM guards relating to GameFSM
 */

#ifndef SERVER017_GUARDS_HPP
#define SERVER017_GUARDS_HPP


#include <spdlog/spdlog.h>
#include <network/messages/GameOperation.hpp>
#include <network/messages/RequestGameOperation.hpp>
#include <util/Player.hpp>

namespace guards {
    struct operationValid {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &, Event const &event) {
            nlohmann::json operationType = event.getType();
            spdlog::debug("Checking GameOperation of type " + operationType.dump() + " ... valid");
            // TODO validate Operation
            return true;
        }
    };

    /**
     * @brief Guard passes if there are no more characters in the remainingCharacters list of a round
     */
    struct noCharactersRemaining {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &) {
            spdlog::debug("Checking guard noCharactersRemaining: {} remaining characters",
                         fsm.remainingCharacters.size());
            return fsm.remainingCharacters.size() == 0;
        }
    };

    /**
     * @brief Guard passes if it's the last choice (together both players have chosen
     *        items over 15 rounds, and one round is missing).
     */
    struct lastChoice {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &) {
            unsigned int missingChoices = 16;
            for (const auto &[_, characterChoice] : state.characterChoices) {
                missingChoices -= characterChoice.size();
            }

            for (const auto &[_, gadgetChoice] : state.gadgetChoices) {
                missingChoices -= gadgetChoice.size();
            }

            spdlog::debug("Checking guard noChoiceMissing: {} remaining choices",
                         missingChoices);
            return missingChoices == 1;
        }
    };

    /**
     * @brief Guard passes if the choice is valid regarding the offer and the already
     *        chosen items.
     */
    struct choiceValid {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &e) {
            spdlog::debug("Checking guard choiceValid");

            auto clientId = e.getClientId();

            const auto &offered = state.offers.at(clientId);

            //TODO: adapt to Role Enum, needs rework of router
            bool validMessage = e.validate(spy::network::RoleEnum::PLAYER, offered.characters, offered.gadgets);
            auto choice = e.getChoice();

            bool choiceInvalid = ((std::holds_alternative<spy::util::UUID>(choice)
                                        && state.characterChoices.at(clientId).size() >= 4)
                                || (std::holds_alternative<spy::gadget::GadgetEnum>(choice)
                                        && state.gadgetChoices.at(clientId).size() >= 6));

            if (validMessage && !choiceInvalid) {
                return true;
            } else {
                //TODO: kick player
                spdlog::critical("Player {} has send an invalid choice and should be kicked", clientId);
                return false;
            }
        }
    };

    /**
     * @brief Guard passes if the equipment choice is valid.
     */
    struct equipmentChoiceValid {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &e) {
            spdlog::debug("Checking guard equipmentChoiceValid");

            auto clientId = e.getClientId();
            bool messageValid = e.validate(spy::network::RoleEnum::PLAYER, state.chosenCharacters.at(clientId), state.chosenGadgets.at(clientId));

            if (messageValid) {
                return true;
            } else {
                //TODO: kick player
                spdlog::critical("Player {} has send an invalid equipment choice and should be kicked", clientId);
                return false;
            }
        }
    };

    /**
     * @brief Guard passes if it's the last equipment choice (one player has submitted his choice, the
     *        other one is missing).
     */
    struct lastEquipmentChoice {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &state, Event const &) {
            const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;

            auto idP1 = playerIds.at(Player::one);
            auto idP2 = playerIds.at(Player::two);

            return (state.chosenCharacters.at(idP1).empty() || state.chosenCharacters.at(idP2).empty());
        }
    };
}


#endif //SERVER017_GUARDS_HPP
