/**
 * @file Util.cpp
 * @author jonas
 * @date 18.05.20
 * Generic utility functions
 */

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
