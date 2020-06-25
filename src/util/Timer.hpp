/**
 * @file Timer.hpp
 * @author jonas
 * @date 01.06.20
 * Simple timer using threads
 */

#ifndef SERVER017_TIMER_HPP
#define SERVER017_TIMER_HPP


#include <thread>
#include <iostream>
#include <optional>

/**
 * Implements a timer that defers a function call for a specified time. Timer will stop on object destruction.
 */
class Timer {
    public:

        using optionalTimePoint = std::optional<std::chrono::time_point<std::chrono::system_clock>>;

        /**
         * Creates a stopped timer
         */
        Timer() = default;

        Timer(const Timer &other) = delete;

        Timer &operator=(const Timer &other) = delete;

        Timer(Timer &&t) noexcept;

        Timer &operator=(Timer &&t) noexcept;

        ~Timer();

        /**
         * Restarts the timer to execute a function after the specified timeout
         * @param timeout Timer duration
         * @param function Function to execute after timeout
         * @param args Arguments to function
         * @details If the timer is running already, this will stop the old and create a new timer
         */
        template<typename FunctionType, typename Rep, typename Period, typename ...Args>
        void restart(std::chrono::duration<Rep, Period> timeout, FunctionType function, Args &&... args) {
            // Stop old thread
            stop();
            // Create new status variable for new thread
            stopped = std::make_shared<bool>(false);
            startTime = std::chrono::system_clock::now();
            std::thread timerThread{[stopped = this->stopped, timeout, function, args...]() {
                std::this_thread::sleep_for(timeout);
                if (*stopped) {
                    return;
                } else {
                    function(std::forward<Args>(args)...);
                    *stopped = true;
                }
            }};
            timerThread.detach();
        }

        /**
         * @brief Stops the timer.
         * @details Timer thread will not terminate immediately, but will not execute function after time is up.
         */
        void stop();

        [[nodiscard]] bool isRunning() const;

        [[nodiscard]] optionalTimePoint getStartTime() const;

    private:
        std::shared_ptr<bool> stopped = std::make_shared<bool>(true);
        optionalTimePoint startTime;
};


#endif //SERVER017_TIMER_HPP
