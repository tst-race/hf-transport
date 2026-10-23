#ifndef LOG_H
#define LOG_H

#include <string>
#include <sstream>

namespace hf_transport {

extern std::string logPrefix;

enum LogLevel { LL_DEBUG, LL_INFO, LL_WARNING, LL_ERROR };

void logDebug(const std::string& msg);
void logInfo(const std::string& msg);
void logWarning(const std::string& msg);
void logError(const std::string& msg);

// Simple trace macro that just logs function name
#define TRACE_METHOD() \
    do { \
        logDebug(logPrefix + std::string(__func__)); \
    } while (0)

#define TRACE_FUNCTION() \
    do { \
        logDebug(logPrefix + std::string(__func__)); \
    } while (0)

// For logging with custom message
#define LOG_METHOD(msg) \
    do { \
        logDebug(logPrefix + std::string(__func__) + ": " + msg); \
    } while (0)

} // namespace hf_transport

#endif // LOG_H