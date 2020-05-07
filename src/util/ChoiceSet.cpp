/**
 * @file   ChoiceSet.cpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  Implementation of a special data structure for the choice phase.
 */

#include "ChoiceSet.hpp"

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
}

Offer ChoiceSet::requestSelection() {
    std::lock_guard<std::mutex> guard(selectionMutex);

    Offer offer;

    if (characters.size() < 3 || gadgets.size() < 3) {
        throw std::invalid_argument("Not enough selections available!");
    }

    offer.characters.resize(3);
    offer.gadgets.resize(3);

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, characters.size() - 1);
        auto index = randPos(rng);

        auto it = characters.begin();
        std::advance(it, index);

        offer.characters.at(i) = *it;
        characters.erase(it);
    }

    for (auto i = 0; i < 3; i++) {
        std::uniform_int_distribution<unsigned int> randPos(0, gadgets.size() - 1);
        auto index = randPos(rng);

        auto it = gadgets.begin();
        std::advance(it, index);

        offer.gadgets.at(i) = *it;
        gadgets.erase(it);
    }


    return offer;
}

bool ChoiceSet::isOfferPossible() const {
    return (characters.size() > 3 && gadgets.size() > 3);
}
