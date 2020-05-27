//
// Created by jonas on 06.05.20.
//

#ifndef SERVER017_ACTIONS_HPP
#define SERVER017_ACTIONS_HPP

#include <spdlog/spdlog.h>
#include <util/UUID.hpp>
#include <network/messages/GameStarted.hpp>
#include <network/messages/HelloReply.hpp>
#include <network/messages/StatisticsMessage.hpp>
#include <util/Player.hpp>
#include <util/GameLogicUtils.hpp>
#include <network/messages/MetaInformation.hpp>
#include <util/Util.hpp>

namespace actions {

    /**
     * This action combines multiple actions that get called in the order they are listed
     */
    template<typename FirstAction, typename ...Actions>
    struct multiple {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &source, TargetState &target) {
            FirstAction{}(event, fsm, source, target);
            if constexpr (sizeof...(Actions) > 0) {
                multiple<Actions...>{}(event, fsm, source, target);
            }
        }
    };

    struct HelloReply {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            spy::network::messages::Hello &helloMessage = event;
            // Client ID is already assigned here, gets assigned directly after server receives hello callback from network
            spy::network::messages::HelloReply helloReply{
                    helloMessage.getClientId(),
                    fsm.sessionId,
                    fsm.scenarioConfig,
                    fsm.matchConfig,
                    fsm.characterInformations
            };

            spdlog::info("Sending HelloReply to {} ({})", helloMessage.getName(), helloReply.getClientId());
            fsm.router.sendMessage(helloReply);
        }
    };

    /**
     * This action gets called when the first player connects.
     * It assigns a sessionId and makes the player Player::one.
     */
    struct InitializeSession {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            spy::network::messages::Hello &helloMessage = event;

            fsm.sessionId = spy::util::UUID::generate();
            spdlog::info("Initialized session with Id {}", fsm.sessionId);

            // Register first player in Server
            spdlog::info("Player one is now {} ({})", helloMessage.getName(), helloMessage.getClientId());
            std::map<Player, spy::util::UUID> &playerIds = fsm.playerIds;
            playerIds.operator[](Player::one) = helloMessage.getClientId();
            std::map<Player, std::string> &playerNames = fsm.playerNames;
            playerNames.operator[](Player::one) = helloMessage.getName();
        }
    };

    /**
     * This action gets called when the second player connects.
     * It makes the player Player::two and sends a spy::network::messages::GameStarted message.
     */
    struct StartGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            spy::network::messages::Hello &helloMessage = event;

            spdlog::info("Starting Game (action StartGame)");

            // Register second player in server
            spdlog::info("Player two is now {} ({})", helloMessage.getName(), helloMessage.getClientId());
            std::map<Player, spy::util::UUID> &playerIds = fsm.playerIds;
            playerIds.operator[](Player::two) = helloMessage.getClientId();
            std::map<Player, std::string> &playerNames = fsm.playerNames;
            playerNames.operator[](Player::two) = helloMessage.getName();

            spy::network::messages::GameStarted gameStarted{
                    spy::util::UUID{}, // UUID gets set in MessageRouter::sendMessage
                    fsm.playerIds.find(Player::one)->second,
                    fsm.playerIds.find(Player::two)->second,
                    fsm.playerNames.find(Player::one)->second,
                    fsm.playerNames.find(Player::two)->second,
                    fsm.sessionId
            };
            spdlog::debug("PlayerIDs: {}", fmt::json(fsm.playerIds));
            spdlog::info("Sending GameStarted message to player one");
            fsm.router.sendMessage(fsm.playerIds.find(Player::one)->second, gameStarted);
            spdlog::info("Sending GameStarted message to player two");
            fsm.router.sendMessage(fsm.playerIds.find(Player::two)->second, gameStarted);
        }
    };

    struct closeGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            const spy::gameplay::State &state = root_machine(fsm).gameState;

            spdlog::info("Closing game");

            spy::character::FactionEnum winningFaction = spy::util::RoundUtils::determineWinningFaction(state);
            Player winner;
            switch (winningFaction) {
                case spy::character::FactionEnum::PLAYER1:
                    winner = Player::one;
                    break;
                case spy::character::FactionEnum::PLAYER2:
                    winner = Player::two;
                    break;
                default:
                    spdlog::error("Winning faction \"{}\" invalid (assuming player one)", fmt::json(winningFaction));
                    winner = Player::one;
                    break;
            }

            spdlog::info("Winning player is {}", fmt::json(winner));

            std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;

            using spy::network::messages::StatisticsMessage;
            StatisticsMessage statisticsMessage{
                    {},
                    {}, // TODO: statistics (optional requirement)
                    playerIds.at(winner),
                    spy::statistics::VictoryEnum::VICTORY_BY_DRINKING, // TODO: determine victory reason
                    false
            };

            MessageRouter &router = root_machine(fsm).router;
            router.broadcastMessage(statisticsMessage);

            // TODO: Keep replay available for 5 minutes, then close connections

            spdlog::debug("Clearing all connections from router");
            router.clearConnections();
        }
    };

    /**
     * This action replies to the sender of a MetaInformation request with a MetaInformationMessage
     */
    struct sendMetaInformation {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&event, FSM &fsm, SourceState &, TargetState &) {
            using spy::network::messages::MetaInformationKey;
            using spy::network::messages::MetaInformation;
            using spy::character::FactionEnum;

            const spy::network::messages::RequestMetaInformation &metaInformationRequest = event;
            std::map<MetaInformationKey, MetaInformation::Info> information;

            const std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
            const spy::gameplay::State &gameState = root_machine(fsm).gameState;


            for (const auto &key: metaInformationRequest.getKeys()) {
                switch (key) {
                    case MetaInformationKey::CONFIGURATION_SCENARIO:
                        information.emplace(key, root_machine(fsm).scenarioConfig);
                        break;
                    case MetaInformationKey::CONFIGURATION_MATCH_CONFIG:
                        information.emplace(key, root_machine(fsm).matchConfig);
                        break;
                    case MetaInformationKey::CONFIGURATION_CHARACTER_INFORMATION:
                        information.emplace(key, root_machine(fsm).characterInformations);
                        break;
                    case MetaInformationKey::FACTION_PLAYER1:
                        //if not requested by P1 or spectator, continue
                        if (playerIds.at(Player::one) != metaInformationRequest.getClientId()) {
                            // TODO: also allow spectators to request FACTION_PLAYER1
                            continue;
                        }

                        // Send all characters with faction PLAYER1
                        information.emplace(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                            FactionEnum::PLAYER1));
                        break;
                    case MetaInformationKey::FACTION_PLAYER2:
                        //if not requested by P1 or spectator, continue
                        if (playerIds.at(Player::two) != metaInformationRequest.getClientId()) {
                            // TODO: also allow spectators to request FACTION_PLAYER1
                            continue;
                        }

                        // Send all characters with faction PLAYER2
                        information.emplace(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                            FactionEnum::PLAYER2));
                        break;
                    case MetaInformationKey::FACTION_NEUTRAL:
                        // Send all NPCs
                        // TODO: check if requester was spectator, do not send otherwise
                        information.emplace(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                            FactionEnum::NEUTRAL));
                        break;
                    case MetaInformationKey::GADGETS_PLAYER1:
                        if (playerIds.at(Player::one) != metaInformationRequest.getClientId()) {
                            // TODO: also allow spectators to request GADGETS_PLAYER1
                            continue;
                        }

                        information.emplace(key, Util::getFactionGadgets(gameState.getCharacters(),
                                                                         FactionEnum::PLAYER1));
                        break;
                    case MetaInformationKey::GADGETS_PLAYER2:
                        if (playerIds.at(Player::two) != metaInformationRequest.getClientId()) {
                            // TODO: also allow spectators to request GADGETS_PLAYER2
                            continue;
                        }

                        information.emplace(key, Util::getFactionGadgets(gameState.getCharacters(),
                                                                         FactionEnum::PLAYER2));
                        break;
                    default:
                        spdlog::warn("Unsupported MetaInformation key requested: {}.", fmt::json(key));
                        break;
                }
            }

            MetaInformation metaInformationReply{metaInformationRequest.getClientId(), information};
            root_machine(fsm).router.sendMessage(metaInformationReply);
        }
    };
}

#endif //SERVER017_ACTIONS_HPP
