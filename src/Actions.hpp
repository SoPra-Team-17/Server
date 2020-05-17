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
            playerIds.insert({Player::one, helloMessage.getClientId()});
            std::map<Player, std::string> &playerNames = fsm.playerNames;
            playerNames.insert({Player::one, helloMessage.getName()});
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

            // Register second player in server
            spdlog::info("Player two is now {} ({})", helloMessage.getName(), helloMessage.getClientId());
            std::map<Player, spy::util::UUID> &playerIds = fsm.playerIds;
            playerIds.insert({Player::two, helloMessage.getClientId()});
            std::map<Player, std::string> &playerNames = fsm.playerNames;
            playerNames.insert({Player::two, helloMessage.getName()});

            spy::network::messages::GameStarted gameStarted{
                    spy::util::UUID{}, // UUID gets set in MessageRouter::sendMessage
                    fsm.playerIds.find(Player::one)->second,
                    fsm.playerIds.find(Player::two)->second,
                    fsm.playerNames.find(Player::one)->second,
                    fsm.playerNames.find(Player::two)->second,
                    fsm.sessionId
            };
            fsm.router.sendMessage(fsm.playerIds.find(Player::one)->second, gameStarted);
            fsm.router.sendMessage(fsm.playerIds.find(Player::two)->second, gameStarted);
        }
    };

    struct closeGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            const spy::gameplay::State &state = root_machine(fsm).gameState;

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

            spy::network::messages::RequestMetaInformation &metaInformationRequest = event;
            std::map<MetaInformationKey, MetaInformation::Info> information;

            for (const auto &key: metaInformationRequest.getKeys()) {
                switch (key) {
                    case MetaInformationKey::CONFIGURATION_SCENARIO:
                        information.emplace(key, root_machine(fsm).scenarioConfig);
                        break;
                    case MetaInformationKey::CONFIGURATION_MATCH_CONFIG:
                        [[fallthrough]];
                    case MetaInformationKey::CONFIGURATION_CHARACTER_INFORMATION:
                        [[fallthrough]];
                    case MetaInformationKey::GAME_REMAINING_PAUSE_TIME:
                        [[fallthrough]];
                    case MetaInformationKey::FACTION_PLAYER1:
                        [[fallthrough]];
                    case MetaInformationKey::FACTION_PLAYER2:
                        [[fallthrough]];
                    case MetaInformationKey::FACTION_NEUTRAL:
                        [[fallthrough]];
                    case MetaInformationKey::GADGETS_PLAYER1:
                        [[fallthrough]];
                    case MetaInformationKey::GADGETS_PLAYER2:
                        [[fallthrough]];
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
