//
// Created by jonas on 09.05.20.
//

#include "util/Operation.hpp"
#include "Format.hpp"
#include <gameLogic/execution/ActionExecutor.hpp>
#include <spdlog/spdlog.h>

void executeOperation(const std::shared_ptr<const spy::gameplay::BaseOperation>& operation, spy::gameplay::State &state,
                      const spy::MatchConfig &matchConfig,
                      std::vector <std::shared_ptr<const spy::gameplay::BaseOperation>> &operationList) {

    using spy::gameplay::ActionExecutor;

    spdlog::info("Executing {} action", fmt::json(operation->getType()));
    bool operationSuccessful = ActionExecutor::execute(state, operation, matchConfig);

    auto operationWithResult = operation->clone();
    operationWithResult->setSuccessful(operationSuccessful);
    operationList.push_back(operationWithResult);

    // TODO: execute potentially resulting exfiltration, add those to fsm.operations
}
