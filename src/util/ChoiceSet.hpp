/**
 * @file   ChoiceSet.hpp
 * @author Dominik Authaler
 * @date   05.05.2020 (creation)
 * @brief  Declaration of a special data structure for the choice phase.
 */

#ifndef SERVER017_CHOICE_SET_HPP
#define SERVER017_CHOICE_SET_HPP

#include <mutex>
#include <list>
#include <random>

#include "util/UUID.hpp"
#include "datatypes/character/CharacterInformation.hpp"
#include "datatypes/gadgets/GadgetEnum.hpp"

struct Offer {
    std::vector<spy::util::UUID> characters;
    std::vector<spy::gadget::GadgetEnum> gadgets;
};

/**
 * This class implements a simple data structure used during choice phase to select characters and gadgets.
 * @note In contrast to the name the data structure currently doesn't enforce set characteristics, but is
 *       intended to be used as one.
 */
class ChoiceSet {
    public:

        ChoiceSet() = default;
        /**
         * Constructs the choice set by only using the uuids of the character information data structure.
         * @param charInfos   CharacterInformation structures of the selectable characters.
         * @param gadgetTypes Types of the selectable gadgets.
         */
        ChoiceSet(const std::vector<spy::character::CharacterInformation> &charInfos,
                  std::list<spy::gadget::GadgetEnum> gadgetTypes);
        /**
         * Constructs the choice set from the two given lists.
         * @param characterIds List of character uuids.
         * @param gadgetTypes  List of gadget types.
         */
        ChoiceSet(std::list<spy::util::UUID> characterIds,
                  std::list<spy::gadget::GadgetEnum> gadgetTypes);

        /**
         * Adds the uuid of the given character information to the selection set.
         * @param c CharacterInformation to add.
         */
        void addForSelection(spy::character::CharacterInformation c);

        /**
         * Adds the given uuid to the selection set.
         * @param u uuid to add.
         */
        void addForSelection(spy::util::UUID u);

        /**
         * Adds the given gadget type to the selection set.
         * @param g Gadget to add.
         */
        void addForSelection(spy::gadget::GadgetEnum g);

        /**
         * Adds the given lists to the respective selection sets.
         * @param chars         Character uuids to add.
         * @param gadgetTypes   Gadgets to add.
         */
        void addForSelection(std::vector<spy::util::UUID> chars, std::vector<spy::gadget::GadgetEnum> gadgetTypes);

        /**
         * Adds the given lists to the respective selection sets.
         * @param chars         Character informations to add. Only the uuid is added.
         * @param gadgetTypes   Gadgets to add.
         */
        void addForSelection(std::vector<spy::character::CharacterInformation> chars, std::vector<spy::gadget::GadgetEnum> gadgetTypes);

        /**
         * Chooses three character uuids and three gadget types which are removed from the set and returned.
         * @return Pair of three character uuids and three gadget types.
         */
        Offer requestSelection();

        /**
         * Chooses three character uuids which are removed from the set and returned.
         * @return Pair of three character uuids.
         */
        Offer requestCharacterSelection();

        /**
         * Chooses three gadget types which are removed from the set and returned.
         * @return Pair of three gadget types.
         */
        Offer requestGadgetSelection();

        /**
         * Checks if the set contains enough items to start one offer.
         * @return True if has more than three gadgets and three characters, else false.
         */
        [[nodiscard]] bool isOfferPossible() const;

        /**
        * Checks if the set contains enough items to start one offer.
        * @return True if has more than three gadgets and three characters, else false.
        */
        [[nodiscard]] bool isCharacterOfferPossible() const;

        /**
        * Checks if the set contains enough items to start one offer.
        * @return True if has more than three gadgets and three characters, else false.
        */
        [[nodiscard]] bool isGadgetOfferPossible() const;

        /**
         * Getter for the number of character uuids within the set.
         * @return Number of character uuids currently in the set.
         */
        [[nodiscard]] unsigned int getNumberOfCharacters() const;

        /**
         * Getter for the number of gadget types within the set.
         * @return Number of gadget types currently in the set.
         */
        [[nodiscard]] unsigned int getNumberOfGadgets() const;


    private:
        std::list<spy::util::UUID> characters;
        std::list<spy::gadget::GadgetEnum> gadgets;

        std::random_device rd;
        std::mt19937 rng;

        mutable std::mutex selectionMutex;              ///< Mutable to be able to lock even in a const method
};


#endif //SERVER017_CHOICE_SET_HPP
