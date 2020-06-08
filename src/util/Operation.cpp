//
// Created by jonas on 09.05.20.
//

#include "util/Operation.hpp"
#include "Format.hpp"
#include <gameLogic/execution/ActionExecutor.hpp>
#include <gameLogic/generation/ActionGenerator.hpp>
#include <spdlog/spdlog.h>

void executeOperation(const std::shared_ptr<const spy::gameplay::BaseOperation>& operation, spy::gameplay::State &state,
                      const spy::MatchConfig &matchConfig,
                      std::vector <std::shared_ptr<const spy::gameplay::BaseOperation>> &operationList,
                      std::deque<spy::util::UUID> remainingCharacters) {

    using spy::gameplay::ActionExecutor;

    spdlog::info("Executing {} action", fmt::json(operation->getType()));
    auto operationWithResult = ActionExecutor::execute(state, operation, matchConfig);

    operationList.push_back(operationWithResult);

    for (auto &c : state.getCharacters()) {
        if (c.getHealthPoints() <= 0) {
            auto exfiltration = spy::gameplay::ActionGenerator::generateExfiltration(state, c.getCharacterId());
            auto exfiltrationResult = ActionExecutor::execute(state, exfiltration, matchConfig);

            operationList.push_back(exfiltrationResult);

            // erase character from remaining characters list
            auto it = std::find(remainingCharacters.begin(), remainingCharacters.end(), c.getCharacterId());
            if (it != remainingCharacters.end()) {
                remainingCharacters.erase(it);
                c.setActionPoints(0);
                c.setMovePoints(0);
            }
        }
    }
}
