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
#include <network/messages/GamePause.hpp>
#include <network/messages/GameStatus.hpp>
#include <util/Player.hpp>
#include <util/GameLogicUtils.hpp>
#include <network/messages/MetaInformation.hpp>
#include <util/Util.hpp>
#include "Events.hpp"

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
                    root_machine(fsm).sessionId,
                    root_machine(fsm).scenarioConfig,
                    root_machine(fsm).matchConfig,
                    root_machine(fsm).characterInformations
            };

            spdlog::info("Sending HelloReply to {} ({})", helloMessage.getName(), helloReply.getClientId());
            root_machine(fsm).router.sendMessage(helloReply);
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

    /**
     * Send GameStarted message to the player sending the event.
     * This is needed after reconnect of the specified player.
     */
    struct sendReconnectGameStart {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(const Event &event, FSM &fsm, SourceState &, TargetState &) {
            spy::network::messages::GameStarted gameStarted{
                    event.getClientId(),
                    root_machine(fsm).playerIds.find(Player::one)->second,
                    root_machine(fsm).playerIds.find(Player::two)->second,
                    root_machine(fsm).playerNames.find(Player::one)->second,
                    root_machine(fsm).playerNames.find(Player::two)->second,
                    root_machine(fsm).sessionId
            };
            spdlog::info("Sending GameStarted message to {}", event.getClientId());
            root_machine(fsm).router.sendMessage(gameStarted);
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

            spdlog::debug("Resetting the game state for the next game");
            root_machine(fsm).gameState = spy::gameplay::State{0, spy::scenario::FieldMap{
                    root_machine(fsm).scenarioConfig}, {}, {}, std::nullopt,
                                                               std::nullopt};
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
            const std::map<spy::util::UUID, spy::network::RoleEnum> &clientRoles = root_machine(fsm).clientRoles;

            spdlog::info("Process Meta Information request");

            bool gameRunning = root_machine(fsm).isIngame;
            auto clientId = metaInformationRequest.getClientId();
            auto clientRole = clientRoles.find(clientId);
            if (clientRole == clientRoles.end()) {
                spdlog::warn("Unregistered client requested meta information --> rejected");
                return;
            }

            bool isSpectator = (clientRole->second == spy::network::RoleEnum::SPECTATOR);
            std::optional<Player> player;
            if (!isSpectator) {
                player = (playerIds.at(Player::one) == clientId) ? Player::one : Player::two;
            }

            for (const auto &key: metaInformationRequest.getKeys()) {
                auto result = Util::handleMetaRequestKey(key, fsm, gameRunning, isSpectator, player);

                if (result.has_value()) {
                    information.insert(result.value());
                }
            }

            MetaInformation metaInformationReply{metaInformationRequest.getClientId(), information};
            root_machine(fsm).router.sendMessage(metaInformationReply);
        }
    };

    template<bool forced = false>
    struct pauseGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &target) {
            target.serverEnforced = forced;

            spdlog::info("Pausing game, serverEnforced={}", forced);
            MessageRouter &router = root_machine(fsm).router;
            router.broadcastMessage(spy::network::messages::GamePause{{}, true, forced});
        }
    };

    struct unpauseGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            bool isForced = std::is_same<Event, events::forceUnpause>::value;

            spdlog::info("Unpausing, forced={}", isForced);
            MessageRouter &router = root_machine(fsm).router;
            router.broadcastMessage(spy::network::messages::GamePause{{}, false, isForced});
        }
    };

    struct startReconnectTimer {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(const Event &event, FSM &fsm, SourceState &, TargetState &target) {
            const events::playerDisconnect &disconnectEvent = event;

            if (target.pauseLimitTimer.isRunning()) {
                // A normal pause is already in progress. We transition to forced pause and halt the pauseLimitTimer.
                // The pause will be continued with the remaining time once everyone reconnected.
                target.pauseLimitTimer.stop();
                auto now = std::chrono::system_clock::now();
                // startTime has value because timer was just running.
                auto pauseStart = target.pauseLimitTimer.getStartTime().value();
                target.pauseTimeRemaining = now - pauseStart;
                // TODO send forced pause message
            }

            auto playerOne = root_machine(fsm).playerIds.find(Player::one);
            if (playerOne == root_machine(fsm).playerIds.end()) {
                spdlog::error("ID of player one not found. Can not determine which player disconnected.");
                return;
            }
            spy::util::UUID playerOneId = playerOne->second;

            const spy::MatchConfig &matchConfig = root_machine(fsm).matchConfig;

            auto reconnectLimit = std::chrono::seconds{
                    matchConfig.getReconnectLimit().value_or(std::chrono::seconds::max().count())};

            auto reconnectTimerEnd = []() {
                spdlog::info("Reconnect timeout reached. Game is now over.");
                // TODO: end game
            };

            if (disconnectEvent.clientId == playerOneId) {
                spdlog::info("Starting reconnect timer for player one for {} seconds",
                             std::chrono::duration_cast<std::chrono::seconds>(reconnectLimit).count());
                target.playerOneReconnectTimer.restart(reconnectLimit, reconnectTimerEnd);
            } else {
                spdlog::info("Starting reconnect timer for player two for {} seconds",
                             std::chrono::duration_cast<std::chrono::seconds>(reconnectLimit).count());
                target.playerTwoReconnectTimer.restart(reconnectLimit, reconnectTimerEnd);
            }
        }
    };

    struct stopReconnectTimer {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(const Event &event, FSM &fsm, SourceState &, TargetState &target) {
            const spy::network::messages::Reconnect &reconnectEvent = event;

            auto playerOne = root_machine(fsm).playerIds.find(Player::one);
            if (playerOne == root_machine(fsm).playerIds.end()) {
                spdlog::error("ID of player one not found. Can not determine which player reconnected.");
                return;
            }
            spy::util::UUID playerOneId = playerOne->second;

            if (playerOneId == reconnectEvent.getClientId()) {
                spdlog::info("Stopping reconnect timer of player one.");
                target.playerOneReconnectTimer.stop();
            } else {
                spdlog::info("Stopping reconnect timer of player two.");
                target.playerTwoReconnectTimer.stop();
            }
        }
    };

    struct revertToNormalPause {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &target) {
            std::chrono::system_clock::duration remainingPauseTime = target.pauseTimeRemaining;
            spdlog::info("Reverting to normal pause.");

            target.serverEnforced = false;

            if (remainingPauseTime > std::chrono::seconds{0}) {
                spdlog::info("Restarting pause timer with {} seconds remaining.",
                             std::chrono::duration_cast<std::chrono::seconds>(remainingPauseTime).count());

                target.pauseLimitTimer.restart(remainingPauseTime, [&fsm]() {
                    spdlog::info("Pause time limit reached, unpausing.");
                    root_machine(fsm).process_event(events::forceUnpause{});
                });

                spdlog::info("Broadcasting that pause is not serverEnforced anymore.");
                spy::network::messages::GamePause pauseMessage{{}, true, false};
            } else {
                spdlog::warn("revertToNormalPause has been called, but a normal pause was not in progress."
                             "Doing nothing.");
            }
        }
    };
}

#endif //SERVER017_ACTIONS_HPP
