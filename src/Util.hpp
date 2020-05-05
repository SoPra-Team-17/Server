//
// Created by jonas on 05.05.20.
//

#ifndef SERVER017_UTIL_HPP
#define SERVER017_UTIL_HPP


#include <network/messages/GameOperation.hpp>

class Util {
    public:
        static void handleOperationFunc(const spy::network::messages::GameOperation &);
};


#endif //SERVER017_UTIL_HPP
