/**
 * @file Operation.hpp
 * @author ottojo
 * Utility functions
 */

#ifndef SERVER017_OPERATION_HPP
#define SERVER017_OPERATION_HPP

#include <network/messages/GameOperation.hpp>

/**
 * Applies the specified operation to the state,
 * evaluates its success,
 * adds it to the operation list,
 * executes resulting operations
 * @param operation     operation to execute
 * @param state         game state
 * @param matchConfig   current match config
 * @param operationList list of executed operations
 */
void executeOperation(const std::shared_ptr<const spy::gameplay::BaseOperation> &operation,
                      spy::gameplay::State &state,
                      const spy::MatchConfig &matchConfig,
                      std::vector<std::shared_ptr<const spy::gameplay::BaseOperation>> &operationList);

#endif //SERVER017_OPERATION_HPP
