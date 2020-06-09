/**
 * @file UUIDNotFoundException.hpp
 * @author jonas
 * @date 6/9/20
 * Exception that shall be thrown when a UUID is not found
 */

#ifndef SERVER017_UUIDNOTFOUNDEXCEPTION_HPP
#define SERVER017_UUIDNOTFOUNDEXCEPTION_HPP


#include <exception>

/**
 * Exception that shall be thrown when a UUID is not found
 */
class UUIDNotFoundException : public std::exception {
    public:
        [[nodiscard]] const char *what() const noexcept override {
            return "UUID not found.";
        }
};


#endif //SERVER017_UUIDNOTFOUNDEXCEPTION_HPP
