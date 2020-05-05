//
// Created by jonas on 05.05.20.
//

#include <spdlog/spdlog.h>
#include "Util.hpp"

void Util::handleOperationFunc(const spy::network::messages::GameOperation &) {
    spdlog::info("Handling some operation");
}
