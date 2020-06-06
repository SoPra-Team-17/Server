/**
 * @file Util.cpp
 * @author jonas
 * @date 18.05.20
 * Generic utility functions
 */

#include <spdlog/spdlog.h>
#include <network/MessageRouter.hpp>
#include "Util.hpp"

auto Util::getFactionGadgets(const spy::character::CharacterSet &characters,
                             spy::character::FactionEnum faction) -> std::vector<spy::gadget::GadgetEnum> {
    std::vector<spy::gadget::GadgetEnum> gadgets;
    for (const auto &character: characters) {
        // if character belongs to faction
        // add it's gadgets to list
        if (character.getFaction() == faction) {
            for (const auto &gadget: character.getGadgets()) {
                gadgets.emplace_back(gadget->getType());
            }
        }
    }
    return gadgets;
}

auto Util::getFactionCharacters(const spy::character::CharacterSet &characters,
                                spy::character::FactionEnum faction) -> std::vector<spy::util::UUID> {
    std::vector<spy::util::UUID> characterUUIDs;
    // copy all characters with faction to characters
    for (const spy::character::Character &character: characters) {
        if (character.getFaction() == faction) {
            characterUUIDs.push_back(character.getCharacterId());
        }
    }
    return characterUUIDs;
}

bool Util::isDisconnectedPlayer(const spy::util::UUID &clientId,
                                const std::map<Player, spy::util::UUID> &playerIds,
                                const MessageRouter &router) {
    if (clientId != playerIds.at(Player::one) and clientId != playerIds.at(Player::two)) {
        spdlog::warn("Received reconnect message from {}, who is not a player in this game.", clientId);
        return false;
    }
    if (router.isConnected(clientId)) {
        spdlog::warn("Received connect message from {}, who is still connected.", clientId);
        return false;
    }
    return true;
}
