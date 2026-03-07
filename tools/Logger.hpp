#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <iostream>

enum class LogLevel {
    Info,
    Error
};

inline void Log(const std::string& message, const LogLevel level = LogLevel::Info) {
    switch (level) {
        case LogLevel::Info:
            std::cout << message << std::endl;
            break;
        case LogLevel::Error:
            std::cerr << "[ERROR] " << message << std::endl;
            break;
    }
}

#endif //LOGGER_HPP_
