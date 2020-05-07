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
     * @brief Guard passes if the client has chosen enough characters and gadgets.
     */
    struct noChoiceMissing {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &) {
            unsigned int missingChoices = 16;
            for (auto it = state.characterChoices.begin(); it != state.characterChoices.end(); it++) {
                missingChoices -= it->second.size();
            }

            for (auto it = state.gadgetChoices.begin(); it != state.gadgetChoices.end(); it++) {
                missingChoices -= it->second.size();
            }

            spdlog::debug("Checking guard noChoiceMissing: {} remaining choices",
                          missingChoices);
            return missingChoices == 0;
        }
    };

    /**
     * @brief Guard passes if the client has chosen enough characters and gadgets.
     */
    struct lastChoice {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &) {
            unsigned int missingChoices = 16;
            for (auto it = state.characterChoices.begin(); it != state.characterChoices.end(); it++) {
                missingChoices -= it->second.size();
            }

            for (auto it = state.gadgetChoices.begin(); it != state.gadgetChoices.end(); it++) {
                missingChoices -= it->second.size();
            }

            spdlog::debug("Checking guard noChoiceMissing: {} remaining choices",
                         missingChoices);
            return missingChoices == 1;
        }
    };

    /**
     * @brief Guard passes if it's the last choice.
     */
    struct choiceValid {
        template<typename FSM, typename FSMState, typename Event>
        bool operator()(FSM const &, FSMState const &state, Event const &e) {
            spdlog::debug("Checking guard choiceValid");

            auto clientId = e.getclientId();

            auto offered = state.offers.at(clientId);

            //TODO: adapt to Role Enum, needs rework of router
            bool validMessage = e.validate(spy::network::RoleEnum::PLAYER, offered.characters, offered.gadgets);
            auto choice = e.getChoice();

            bool choiceInvalid = ((std::holds_alternative<spy::util::UUID>(choice)
                                        && state.characterChoices.at(clientId).size() >= 4)
                                || (std::holds_alternative<spy::gadget::GadgetEnum>(choice)
                                        && state.gadgetChoices.at(clientId).size() >= 6));

            if (validMessage && !choiceInvalid) {
                spdlog::debug("Choice valid!");
                return true;
            } else {
                //TODO: kick player
                spdlog::critical("Player {} has send an invalid choice and was therefore kicked", clientId);
                return false;
            }
        }
    };
}


#endif //SERVER017_GUARDS_HPP
