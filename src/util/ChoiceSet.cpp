/**
 * @file   ChoiceSet.cpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  Implementation of a special data structure for the choice phase.
 */

#include "ChoiceSet.hpp"

#include <random>

ChoiceSet::ChoiceSet(const std::vector<spy::character::CharacterInformation> &charInfos,
                     std::list<spy::gadget::GadgetEnum> gadgetTypes) : gadgets(std::move(gadgetTypes)) {
    std::copy(charInfos.begin(), charInfos.end(), std::back_inserter(characters));
}

ChoiceSet::ChoiceSet(std::list<spy::util::UUID> characterIds,
                     std::list<spy::gadget::GadgetEnum> gadgetTypes) : characters(std::move(characterIds)),
                                                                       gadgets(std::move(gadgetTypes)) {

}

void ChoiceSet::addForSelection(spy::character::CharacterInformation c) {
    characters.push_back(c.getCharacterId());
}

void ChoiceSet::addForSelection(spy::util::UUID u) {
    characters.push_back(u);
}

void ChoiceSet::addForSelection(spy::gadget::GadgetEnum g) {
    gadgets.push_back(g);
}

void ChoiceSet::addForSelection(std::vector<spy::util::UUID> chars, std::vector<spy::gadget::GadgetEnum> gadgetTypes) {
    std::copy(chars.begin(), chars.end(), std::back_inserter(characters));
    std::copy(gadgetTypes.begin(), gadgetTypes.end(), std::back_inserter(gadgets));
}

std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>> ChoiceSet::requestSelection() {
    std::lock_guard<std::mutex> g(selectionMutex);

    std::vector<spy::util::UUID> selectedChars(3);
    std::vector<spy::gadget::GadgetEnum> selectedGadgets(3);

    std::random_device rd;
    std::mt19937 gen(rd());

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, characters.size() - 1);
        auto index = randPos(gen);

        auto it = characters.begin();
        std::advance(it, index);

        selectedChars.at(i) = *it;
        characters.erase(it);
    }

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, gadgets.size() - 1);
        auto index = randPos(gen);

        auto it = gadgets.begin();
        std::advance(it, index);

        selectedGadgets.at(i) = *it;
        gadgets.erase(it);
    }


    return std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>>(selectedChars,
                                                                                         selectedGadgets);
}
