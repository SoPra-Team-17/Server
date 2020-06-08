//
// Created by jonas on 06.05.20.
//

#ifndef SERVER017_ACTIONS_HPP
#define SERVER017_ACTIONS_HPP

#include <spdlog/spdlog.h>
#include <network/messages/GameStarted.hpp>
#include <network/messages/HelloReply.hpp>
#include <network/messages/StatisticsMessage.hpp>
#include <network/messages/GamePause.hpp>
#include <network/messages/MetaInformation.hpp>
#include <network/messages/GameLeft.hpp>
#include <util/Player.hpp>
#include <util/GameLogicUtils.hpp>
#include <util/Util.hpp>
#include <util/UUID.hpp>
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

    /**
     * Closes the current game.
     */
    struct closeGame {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&, FSM &fsm, SourceState &, TargetState &) {
            using spy::character::FactionEnum;
            using spy::statistics::VictoryEnum;
            using spy::network::messages::StatisticsMessage;
            using spy::statistics::Statistics;
            using spy::statistics::StatisticsEntry;
            using spy::gameplay::Stats;

            const spy::gameplay::State &state = root_machine(fsm).gameState;
            MessageRouter &router = root_machine(fsm).router;
            std::map<Player, spy::util::UUID> &playerIds = root_machine(fsm).playerIds;
            Stats gameStats = root_machine(fsm).gameState.getFactionStats();

            spdlog::info("Closing game");

            FactionEnum winningFaction;
            VictoryEnum victoryReason;
            Player winner;

            // check if the game ended through a player's leave
            if (!router.isConnected(playerIds.at(Player::one))) {
                winner = Player::two;
                victoryReason = VictoryEnum::VICTORY_BY_LEAVE;
            } else if (!router.isConnected(playerIds.at(Player::two))) {
                winner = Player::one;
                victoryReason = VictoryEnum::VICTORY_BY_LEAVE;
            } else {
                std::tie(winningFaction, victoryReason) = spy::util::RoundUtils::determineVictory(state);
                // if there is no clear winner, player one has won the game!
                if (winningFaction != FactionEnum::INVALID) {
                    winner = (winningFaction == FactionEnum::PLAYER2) ? Player::two : Player::one;
                } else {
                    spdlog::error("Winning faction \"{}\" invalid (assuming player one)", fmt::json(winningFaction));
                    winner = Player::one;
                }

            }

            spdlog::info("Winning player is {}", fmt::json(winner));

            Statistics stats;

            stats.addEntry(StatisticsEntry{"Damage suffered", "Suffered damage of the factions",
                                           std::to_string(gameStats.damageSuffered.first),
                                           std::to_string(gameStats.damageSuffered.second)});

            stats.addEntry(StatisticsEntry{"Drunk cocktails",
                                           "Number of cocktails the factions drunk",
                                           std::to_string(gameStats.cocktails.first),
                                           std::to_string(gameStats.cocktails.second)});

            stats.addEntry(StatisticsEntry{"Poured cocktails",
                                    "Number of cocktails the factions poured over other characters",
                                    std::to_string(gameStats.cocktailsPoured.first),
                                    std::to_string(gameStats.cocktailsPoured.second)});

            StatisticsMessage statisticsMessage{
                    {},
                    stats,
                    playerIds.at(winner),
                    victoryReason,
                    false
            };

            router.broadcastMessage(statisticsMessage);

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

    /**
     * Sends a game left message to the client that wants to leave the game.
     * @note Only intended to be used for spectators.
     */
    struct sendGameLeft {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &, TargetState &) {
            auto clientId = e.getClientId();

            MessageRouter &router = root_machine(fsm).router;
            spy::network::messages::GameLeft gameLeft(clientId, clientId);

            router.sendMessage(gameLeft);

            router.closeConnection(clientId);
        }
    };

    /**
     * Broadcasts a game left message to all registered clients.
     */
    struct broadcastGameLeft {
        template<typename Event, typename FSM, typename SourceState, typename TargetState>
        void operator()(Event &&e, FSM &fsm, SourceState &, TargetState &) {
            spy::util::UUID clientId;
            if constexpr (std::is_same<Event, spy::network::messages::GameLeave>::value) {
                clientId = e.getClientId();
            } else if constexpr(std::is_same<Event, events::playerDisconnect>::value) {
                clientId = e.clientId;
            }

            spdlog::debug("Broadcasting leave of client: {}", clientId);

            MessageRouter &router = root_machine(fsm).router;
            spy::network::messages::GameLeft gameLeft({}, clientId);

            router.broadcastMessage(gameLeft);

            router.closeConnection(clientId);
        }
    };
}

#endif //SERVER017_ACTIONS_HPP
