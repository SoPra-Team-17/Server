/**
 * @file Timer.cpp
 * @author jonas
 * @date 6/4/20
 * Simple timer using threads
 */

#include "Timer.hpp"

void Timer::stop() {
    // stopped might be nullptr after the timer has been moved and the old instance gets destructed
    if (stopped != nullptr) {
        *stopped = true;
    }
}

bool Timer::isRunning() const {
    return not(*stopped);
}

Timer &Timer::operator=(Timer &&t) noexcept {
    stop();
    stopped = std::move(t.stopped);
    return *this;
}

Timer::Timer(Timer &&t) noexcept: stopped{std::move(t.stopped)} {}

Timer::~Timer() {
    stop();
}
