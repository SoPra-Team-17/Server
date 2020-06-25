//
// Created by jonas on 04.05.20.
//

#ifndef SERVER017_PLAYER_HPP
#define SERVER017_PLAYER_HPP

#include <ostream>

enum class Player : int {
        one = 0,
        two
};

std::ostream &operator<<(std::ostream &os, Player p);

#endif //SERVER017_PLAYER_HPP
