/**
 * @file   ChoiceSet.cpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  Implementation of a special data structure for the choice phase.
 */

#include "ChoiceSet.hpp"

#include <iostream>

ChoiceSet::ChoiceSet(const std::vector<spy::character::CharacterInformation> &charInfos,
                     std::list<spy::gadget::GadgetEnum> gadgetTypes) : gadgets(std::move(gadgetTypes)), rng(rd()) {
    for (const auto &c : charInfos) {
        characters.push_back(c.getCharacterId());
    }
}

ChoiceSet::ChoiceSet(std::list<spy::util::UUID> characterIds,
                     std::list<spy::gadget::GadgetEnum> gadgetTypes) : characters(std::move(characterIds)),
                                                                       gadgets(std::move(gadgetTypes)), rng(rd()) {

}

void ChoiceSet::addForSelection(spy::character::CharacterInformation c) {
    std::lock_guard<std::mutex> guard(selectionMutex);
    characters.push_back(c.getCharacterId());
}

void ChoiceSet::addForSelection(spy::util::UUID u) {
    std::lock_guard<std::mutex> guard(selectionMutex);
    characters.push_back(u);
}

void ChoiceSet::addForSelection(spy::gadget::GadgetEnum g) {
    std::lock_guard<std::mutex> guard(selectionMutex);
    gadgets.push_back(g);
}

void ChoiceSet::addForSelection(std::vector<spy::util::UUID> chars, std::vector<spy::gadget::GadgetEnum> gadgetTypes) {
    std::lock_guard<std::mutex> g(selectionMutex);
    std::copy(chars.begin(), chars.end(), std::back_inserter(characters));
    std::copy(gadgetTypes.begin(), gadgetTypes.end(), std::back_inserter(gadgets));
}

void ChoiceSet::addForSelection(std::vector<spy::character::CharacterInformation> chars,
                                std::vector<spy::gadget::GadgetEnum> gadgetTypes) {
    std::lock_guard<std::mutex> g(selectionMutex);
    for (const auto &c : chars) {
        characters.push_back(c.getCharacterId());
    }
    std::copy(gadgetTypes.begin(), gadgetTypes.end(), std::back_inserter(gadgets));

    std::cout << "Chars: " << chars.size() << std::endl;
    std::cout << "gadgetTypes: " << chars.size() << std::endl;
    std::cout << "Characters: " << characters.size() << std::endl;
    std::cout << "Gadgets: " << gadgets.size() << std::endl;
}

std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>> ChoiceSet::requestSelection() {
    std::lock_guard<std::mutex> guard(selectionMutex);

    if (characters.size() < 3 || gadgets.size() < 3) {
        throw std::invalid_argument("Not enough selections available!");
    }

    std::vector<spy::util::UUID> selectedChars(3);
    std::vector<spy::gadget::GadgetEnum> selectedGadgets(3);

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, characters.size() - 1);
        auto index = randPos(rng);

        auto it = characters.begin();
        std::advance(it, index);

        selectedChars.at(i) = *it;
        characters.erase(it);
    }

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, gadgets.size() - 1);
        auto index = randPos(rng);

        auto it = gadgets.begin();
        std::advance(it, index);

        selectedGadgets.at(i) = *it;
        gadgets.erase(it);
    }


    return std::pair<std::vector<spy::util::UUID>, std::vector<spy::gadget::GadgetEnum>>(selectedChars,
                                                                                         selectedGadgets);
}

bool ChoiceSet::isSelectionPossible() const {
    return (characters.size() > 3 && gadgets.size() > 3);
}
