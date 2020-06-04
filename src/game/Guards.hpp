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
#include <util/Format.hpp>
#include <gameLogic/validation/ActionValidator.hpp>
#include <util/RoundUtils.hpp>

namespace guards {
    struct operationValid {
        template<typename FSM, typename FSMState>
        bool operator()(FSM const &fsm, FSMState const &, const spy::network::messages::GameOperation &event) {
            spdlog::debug("Checking GameOperation of type {}", fmt::json(event.getType()));

            using spy::gameplay::ActionValidator;
            const spy::gameplay::State &state = root_machine(fsm).gameState;

            return ActionValidator::validate(state, event.getOperation(), root_machine(fsm).matchConfig);
        }
    };

    /**
     * @brief Guard passes if there are characters in the remainingCharacters list of a round
     */
    struct charactersRemaining {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &) {
            spdlog::debug("Checking guard noCharactersRemaining: {} remaining characters",
                          fsm.remainingCharacters.size());
            return !fsm.remainingCharacters.empty();
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
            for (const auto &[_, choiceCount] : state.choiceCount) {
                missingChoices -= choiceCount;
            }

            spdlog::debug("Checking guard lastChoice: {} remaining choices",
                          missingChoices);
            return (missingChoices == 1);
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

            bool alreadyChosen = state.hasChosen.at(clientId);

            bool messageValid = e.validate(spy::network::RoleEnum::PLAYER,
                                           state.chosenCharacters.at(clientId),
                                           state.chosenGadgets.at(clientId));

            if (!alreadyChosen && messageValid) {
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

            return (state.hasChosen.at(idP1) || state.hasChosen.at(idP2));
        }
    };

    struct gameOver {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &) {
            spdlog::debug("Testing GameOver condition");
            return root_machine(fsm).isIngame && spy::util::RoundUtils::isGameOver(root_machine(fsm).gameState);
        }
    };

    /**
     * @brief Guard passes if RequestGamePause message is a valid pause request
     */
    struct isPauseRequest {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &event) {
            const spy::network::messages::RequestGamePause &pauseRequest = event;
            return pauseRequest.validate(root_machine(fsm).clientRoles.at(pauseRequest.getClientId()),
                                         false,
                                         false);
        }
    };

    /**
     * @brief Guard passes if RequestGamePause message is a valid unpause request
     */
    struct isUnPauseRequest {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &state, Event const &event) {
            const spy::network::messages::RequestGamePause &pauseRequest = event;
            return pauseRequest.validate(root_machine(fsm).clientRoles.at(pauseRequest.getClientId()),
                                         true,
                                         state.serverEnforced);
        }
    };

    /**
     * @brief Guard passes if the chosen role of the client is spectator.
     */
    struct isSpectator {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &fsm, FSMState const &, Event const &e) {
            spdlog::debug("Testing spectator condition");

            const auto &clientRoles = root_machine(fsm).clientRoles;
            const spy::network::messages::Hello &message = e;

            return (clientRoles.at(message.getClientId()) == spy::network::RoleEnum::SPECTATOR);
        }
    };
}


#endif //SERVER017_GUARDS_HPP
