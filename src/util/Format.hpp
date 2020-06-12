//
// Created by jonas on 07.05.20.
//

#ifndef SERVER017_FORMAT_HPP
#define SERVER017_FORMAT_HPP

#include <string>
#include <nlohmann/json.hpp>

namespace fmt {
    template<typename T>
    std::string json(T t, int indent = -1) {
        nlohmann::json j = t;
        return j.dump(indent);
    }
}

#endif //SERVER017_FORMAT_HPP
