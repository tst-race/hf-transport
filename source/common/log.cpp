#include "log.h"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace hf_transport {

std::string logPrefix = "[HFTransport] ";

static LogLevel currentLogLevel = LL_DEBUG;

void logDebug(const std::string& msg) {
    if (currentLogLevel <= LL_DEBUG) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] [DEBUG] " << logPrefix << msg << std::endl;
    }
}

void logInfo(const std::string& msg) {
    if (currentLogLevel <= LL_INFO) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] [INFO] " << logPrefix << msg << std::endl;
    }
}

void logWarning(const std::string& msg) {
    if (currentLogLevel <= LL_WARNING) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] [WARN] " << logPrefix << msg << std::endl;
    }
}

void logError(const std::string& msg) {
    if (currentLogLevel <= LL_ERROR) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cerr << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] [ERROR] " << logPrefix << msg << std::endl;
    }
}

} // namespace hf_transport