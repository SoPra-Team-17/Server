/**
 * @file Util.hpp
 * @author jonas
 * @date 18.05.20
 * Generic utility functions
 */

#ifndef SERVER017_UTIL_HPP
#define SERVER017_UTIL_HPP

#include <datatypes/gadgets/GadgetEnum.hpp>
#include <datatypes/character/CharacterSet.hpp>
#include <network/messages/MetaInformationKey.hpp>
#include <network/messages/MetaInformation.hpp>
#include <datatypes/gameplay/State.hpp>
#include <spdlog/spdlog.h>
#include "Player.hpp"
#include "Format.hpp"

class Util {
        using MetaInformationKey = spy::network::messages::MetaInformationKey;
        using MetaInformation = spy::network::messages::MetaInformation;
        using MetaInformationPair = std::pair<MetaInformationKey, MetaInformation::Info>;

    public:
        /**
         * Get all gadget type the specified faction owns
         */
        static auto getFactionGadgets(const spy::character::CharacterSet &characters,
                                      spy::character::FactionEnum faction) -> std::vector<spy::gadget::GadgetEnum>;

        /**
         * Get all character UUIDs in the specified faction
         */
        static auto getFactionCharacters(const spy::character::CharacterSet &characters,
                                         spy::character::FactionEnum faction) -> std::vector<spy::util::UUID>;

        static bool hasAPMP(const spy::character::Character &character) {
            return character.getActionPoints() > 0 or character.getMovePoints() > 0;
        }

        /**
         *
         * @tparam FSM Type of the used state machine.
         * @param key  Key to query.
         * @param fsm  State machine.
         * @param gameRunning Is game currently running?
         * @param isSpectator Is the requesting client a spectator?
         * @param player Player number if requesting client is not a spectator
         * @return Key value pair if request was successful, else std::nullopt.
         */
        template<typename FSM>
        static auto handleMetaRequestKey(spy::network::messages::MetaInformationKey key,
                                         FSM &fsm,
                                         bool gameRunning,
                                         bool isSpectator,
                                         std::optional<Player> player) -> std::optional<MetaInformationPair> {

            const spy::gameplay::State &gameState = root_machine(fsm).gameState;

            switch (key) {
                case MetaInformationKey::CONFIGURATION_SCENARIO:
                    return MetaInformationPair(key, root_machine(fsm).scenarioConfig);

                case MetaInformationKey::CONFIGURATION_MATCH_CONFIG:
                    return MetaInformationPair(key, root_machine(fsm).matchConfig);

                case MetaInformationKey::CONFIGURATION_CHARACTER_INFORMATION:
                    return MetaInformationPair(key, root_machine(fsm).characterInformations);

                case MetaInformationKey::FACTION_PLAYER1:
                    // request only allowed during game phase by spectators and player 1
                    if (!gameRunning || (!isSpectator && player != Player::one)) {
                        return std::nullopt;
                    }

                    // Send all characters with faction PLAYER1
                    return MetaInformationPair(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                        spy::character::FactionEnum::PLAYER1));

                case MetaInformationKey::FACTION_PLAYER2:
                    // request only allowed during game phase by spectators and player 2
                    if (!gameRunning || (!isSpectator && player != Player::two)) {
                        return std::nullopt;
                    }

                    // Send all characters with faction PLAYER2
                    return MetaInformationPair(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                        spy::character::FactionEnum::PLAYER2));

                case MetaInformationKey::FACTION_NEUTRAL:
                    // request only allowed during game phase by spectators
                    if (!gameRunning || !isSpectator) {
                        return std::nullopt;
                    }

                    // Send all NPCs
                    return MetaInformationPair(key, Util::getFactionCharacters(gameState.getCharacters(),
                                                                        spy::character::FactionEnum::NEUTRAL));

                case MetaInformationKey::GADGETS_PLAYER1:
                    // request only allowed during game phase by spectators and player 1
                    if (!gameRunning || (!isSpectator && player != Player::one)) {
                        return std::nullopt;
                    }

                    return MetaInformationPair(key, Util::getFactionGadgets(gameState.getCharacters(),
                                                                     spy::character::FactionEnum::PLAYER1));

                case MetaInformationKey::GADGETS_PLAYER2:
                    // request only allowed during game phase by spectators and player 2
                    if (!gameRunning || (!isSpectator && player != Player::two)) {
                        return std::nullopt;
                    }

                    return MetaInformationPair(key, Util::getFactionGadgets(gameState.getCharacters(),
                                                                            spy::character::FactionEnum::PLAYER2));

                default:
                    spdlog::warn("Unsupported MetaInformation key requested: {}.", fmt::json(key));
                    return std::nullopt;
            }
        }

        /**
         * Checks if the UUID is a player in the current game and not currently connected
         * @param clientId UUID to check
         * @param playerIds IDs of both current players
         * @param router MessageRouter instance to check if UUID is currently connected
         * @return True if clientId is Player::one or two and clientId is connected to router
         */
        static bool isDisconnectedPlayer(const spy::util::UUID &clientId,
                                         const std::map<Player, spy::util::UUID> &playerIds,
                                         const MessageRouter &router);
};


#endif //SERVER017_UTIL_HPP
