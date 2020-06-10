/**
 * @file Util.cpp
 * @author jonas
 * @date 18.05.20
 * Generic utility functions
 */

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
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

    auto playerOneId = playerIds.find(Player::one);
    auto playerTwoId = playerIds.find(Player::two);
    bool isPlayerOne = (playerOneId != playerIds.end()) and (playerOneId->second == clientId);
    bool isPlayerTwo = (playerTwoId != playerIds.end()) and (playerTwoId->second == clientId);

    if (!isPlayerOne and !isPlayerTwo) {
        spdlog::warn("Received reconnect message from {}, who is not a player in this game.", clientId);
        return false;
    }
    if (router.isConnected(clientId)) {
        spdlog::warn("Received connect message from {}, who is still connected.", clientId);
        return false;
    }
    return true;
}

bool Util::hasMPInFog(const spy::character::Character &character, const spy::gameplay::State &state) {
    if (character.getMovePoints() > 0) {
        return true;
    } else {
        // actions are blocked in the fog, thus the character's turn ends with his last move point
        if(not character.getCoordinates().has_value()){
            spdlog::warn("hasMPInFog: target does not have coordinates");
            return false;
        }

        return !state.getMap().getField(character.getCoordinates().value()).isFoggy();
    }
}
