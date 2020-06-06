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
#include "Player.hpp"

class Util {
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

        static bool isDisconnectedPlayer(const spy::util::UUID &clientId,
                                         const std::map<Player, spy::util::UUID> &playerIds,
                                         const MessageRouter &router);
};


#endif //SERVER017_UTIL_HPP
