/**
 * @file GameFSM.hpp
 * @author ottojo
 * @brief State machine for game
 */

#ifndef SERVER017_GAMEFSM_HPP
#define SERVER017_GAMEFSM_HPP

#include <afsm/fsm.hpp>
#include <network/messages/GameOperation.hpp>
#include <datatypes/character/CharacterInformation.hpp>
#include <network/messages/ItemChoice.hpp>
#include <network/messages/RequestEquipmentChoice.hpp>
#include "Events.hpp"
#include "Guards.hpp"
#include "Actions.hpp"
#include "util/Player.hpp"
#include "spdlog/fmt/ostr.h"
#include "OperationHandling.hpp"
#include "util/ChoiceSet.hpp"
#include "ChoicePhaseFSM.hpp"
#include "EquipChoiceHandling.hpp"

class GameFSM : public afsm::def::state_machine<GameFSM> {
    public:
        ChoicePhase choicePhase;

        struct equipPhase : state<equipPhase> {
            // Maps from clientId to the vector of chosen character uuids
            using CharacterMap = std::map<spy::util::UUID, std::vector<spy::util::UUID>>;
            // Maps from clientId to the vector of chosen gadget types
            using GadgetMap = std::map<spy::util::UUID, std::vector<spy::gadget::GadgetEnum>>;


            CharacterMap chosenCharacters;              ///< Stores the character choice of the players
            GadgetMap chosenGadgets;                 ///< Stores the gadget choice of the players
            std::map<spy::util::UUID, bool> hasChosen;  ///< Stores whether the client has already sent his equip choice

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Entering equip phase");

                const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
                MessageRouter &router = root_machine(fsm).router;

                auto idP1 = playerIds.at(Player::one);
                auto idP2 = playerIds.at(Player::two);

                hasChosen[idP1] = false;
                hasChosen[idP2] = false;

                spy::network::messages::RequestEquipmentChoice messageP1(idP1, chosenCharacters.at(idP1),
                                                                         chosenGadgets.at(idP1));
                spy::network::messages::RequestEquipmentChoice messageP2(idP2, chosenCharacters.at(idP2),
                                                                         chosenGadgets.at(idP2));

                router.sendMessage(messageP1);
                spdlog::info("Sending request for equipment choice to player1 ({})", idP1);
                router.sendMessage(messageP2);
                spdlog::info("Sending request for equipment choice to player2 ({})", idP2);
            }

            // @formatter:off
            using internal_transitions = transition_table <
            //  Event                                   Action                                                     Guard
            in<spy::network::messages::EquipmentChoice, actions::handleEquipmentChoice, and_<not_<guards::lastEquipmentChoice>, guards::equipmentChoiceValid>>
            >;
            // @formatter:on
        };

        struct gamePhase : state_machine<gamePhase> {

            spy::util::UUID activeCharacter; //!< Set in roundInit state and waitingForOperation internal transition

            std::deque<spy::util::UUID> remainingCharacters; //!< Characters that have not made a move this round

            template<typename FSM, typename Event>
            void on_enter(Event &&, FSM &fsm) {
                spdlog::info("Initial entering to game phase");

                root_machine(fsm).isIngame = true;

                spy::gameplay::State &gameState = root_machine(fsm).gameState;
                spy::MatchConfig &config = root_machine(fsm).matchConfig;
                std::mt19937 &rng = root_machine(fsm).rng;
                auto &knownCombinations = root_machine(fsm).knownCombinations;

                knownCombinations[Player::one] = {};
                knownCombinations[Player::two] = {};

                std::vector<unsigned int> safeIndexes;

                gameState.getMap().forAllFields([&safeIndexes](const spy::scenario::Field &f) {
                    if (f.getFieldState() == spy::scenario::FieldStateEnum::SAFE) {
                        safeIndexes.push_back(safeIndexes.size() + 1);
                    }
                });

                std::shuffle(safeIndexes.begin(), safeIndexes.end(), root_machine(fsm).rng);

                auto indexIterator = safeIndexes.begin();

                // Initialize roulette tables with random amount of chips, safes with an index
                gameState.getMap().forAllFields([&rng, &config, &indexIterator](spy::scenario::Field &field) {
                    if (field.getFieldState() == spy::scenario::FieldStateEnum::ROULETTE_TABLE) {
                        std::uniform_int_distribution<unsigned int> randChips(config.getMinChipsRoulette(),
                                                                              config.getMaxChipsRoulette());

                        field.setChipAmount(randChips(rng));
                    } else if (field.getFieldState() == spy::scenario::FieldStateEnum::SAFE) {
                        field.setSafeIndex(*indexIterator);
                        indexIterator++;
                    }
                });

                auto fieldSelector = [&gameState](const auto &p) {
                    // Random free/seat without character

                    auto field = gameState.getMap().getField(p);
                    using spy::scenario::FieldStateEnum;
                    if (field.getFieldState() != FieldStateEnum::BAR_SEAT
                        and field.getFieldState() != FieldStateEnum::FREE) {
                        // Wrong field type
                        return false;
                    }
                    return !spy::util::GameLogicUtils::isPersonOnField(gameState, p);
                };

                // Randomly distribute characters
                spdlog::info("Distributing characters");
                for (auto &character: gameState.getCharacters()) {
                    auto randomField = spy::util::GameLogicUtils::getRandomMapPoint(gameState, fieldSelector);
                    if (!randomField.has_value()) {
                        spdlog::critical("No field to place character");
                        std::exit(1);
                    }
                    spdlog::debug("Placing {} at {}", character.getName(), fmt::json(randomField.value()));
                    character.setCoordinates(randomField.value());
                }

                // place the cat on a random field
                auto randomField = spy::util::GameLogicUtils::getRandomMapPoint(gameState, fieldSelector);
                if (!randomField.has_value()) {
                    spdlog::critical("No field to place the white cat");
                    std::exit(1);
                }
                spdlog::debug("Placing white cat at {}", fmt::json(randomField.value()));
                gameState.setCatCoordinates(randomField.value());

            }

            template<typename FSM, typename Event>
            void on_exit(Event &&, FSM &fsm) {
                spdlog::debug("Exiting state gamePhase");
                root_machine(fsm).isIngame = false;
            }

            /**
             * Last operation + resulting operations (Exfiltration)
             */
            std::vector<std::shared_ptr<const spy::gameplay::BaseOperation>> operations;

            struct roundInit : state<roundInit> {
                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &fsm) {
                    spdlog::info("Entering state roundInit");
                    const auto &characters = root_machine(fsm).gameState.getCharacters();

                    for (const auto &c: characters) {
                        fsm.remainingCharacters.push_back(c.getCharacterId());
                    }
                    std::shuffle(fsm.remainingCharacters.begin(), fsm.remainingCharacters.end(), root_machine(fsm).rng);

                    fsm.activeCharacter = fsm.remainingCharacters.front();

                    spdlog::info("Initialized round order:");
                    for (const auto &uuid: fsm.remainingCharacters) {
                        spdlog::info("{} \t({})", characters.findByUUID(uuid)->getName(), uuid);
                    }
                    spy::gameplay::State &state = root_machine(fsm).gameState;
                    const spy::MatchConfig &matchConfig = root_machine(fsm).matchConfig;

                    using spy::util::RoundUtils;

                    RoundUtils::refillBarTables(state);
                    RoundUtils::updateFog(state);
                    RoundUtils::checkGadgetFailure(state, matchConfig);
                    RoundUtils::resetUpdatedMarker(state);
                    for (auto &character: state.getCharacters()) {
                        RoundUtils::determinePoints(character);
                    }

                    root_machine(fsm).process_event(events::roundInitDone{});
                }
            };

            /**
             * @brief In this state the server has requested an operation for activeCharacter and is waiting for response
             */
            struct waitingForOperation : state<waitingForOperation> {

                template<typename FSM, typename Event>
                void on_enter(Event &&, FSM &) {
                    spdlog::info("Entering state waitingForOperation");
                }

                // @formatter:off
                using internal_transitions = transition_table <
                // Event                                  Action                                                                                               Guard
                in<spy::network::messages::GameOperation, actions::multiple<actions::handleOperation, actions::broadcastState, actions::requestNextOperation>, and_<guards::operationValid, guards::charactersRemaining>>,
                in<events::triggerNPCmove,                actions::multiple<actions::npcMove, actions::broadcastState, actions::requestNextOperation>>
                >;
                // @formatter:on
            };

            using initial_state = roundInit;


            // @formatter:off
            using transitions = transition_table <
            //  Start               Event                                  Next                 Action                                                                      Guard
            tr<roundInit,           events::roundInitDone,                 waitingForOperation, actions::multiple<actions::broadcastState, actions::requestNextOperation>>,
            tr<waitingForOperation, spy::network::messages::GameOperation, roundInit,           actions::multiple<actions::handleOperation, actions::broadcastState>,        not_<guards::charactersRemaining>>,
            tr<waitingForOperation, events::triggerNPCmove,                roundInit,           actions::multiple<actions::npcMove, actions::broadcastState>,                not_<guards::charactersRemaining>>
            >;
            // @formatter:on
        };

        using initial_state = decltype(choicePhase);

        template<typename FSM, typename Event>
        void on_enter(Event &&, FSM &) {
            spdlog::info("Entering Game State");
        }

        // @formatter:off
        using transitions = transition_table <
        // Start                  Event                                    Next        Action                                                                 Guard
        tr<decltype(choicePhase), spy::network::messages::ItemChoice,      equipPhase, actions::multiple<actions::handleChoice, actions::createCharacterSet>, and_<guards::lastChoice, guards::choiceValid>>,
        tr<equipPhase,            spy::network::messages::EquipmentChoice, gamePhase,  actions::handleEquipmentChoice,                                        and_<guards::lastEquipmentChoice, guards::equipmentChoiceValid>>
        >;
        // @formatter:on

        using internal_transitions = transition_table <
        // Event                                           Action
        // Reply to MetaInformation request at any time during the game
        in<spy::network::messages::RequestMetaInformation, actions::sendMetaInformation>>;
};

#endif //SERVER017_GAMEFSM_HPP
