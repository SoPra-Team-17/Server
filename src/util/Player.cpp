//
// Created by jonas on 04.05.20.
//
#include "Player.hpp"

std::ostream &operator<<(std::ostream &os, Player p) {
    switch (p) {
        case Player::one:
            return os << "one";
        case Player::two:
            return os << "two";
    }
    return os;
}
