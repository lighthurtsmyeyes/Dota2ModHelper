#pragma once

#include <string>

class CrashLogger {
public:
    static void LogFatal(
        const std::string& context,
        const std::string& details,
        const char* file = nullptr,
        int line = 0,
        bool terminate = true
    );

    // Non-fatal error: logged to logs/error_<timestamp>.log, program continues.
    static void LogError(
        const std::string& context,
        const std::string& details,
        const char* file = nullptr,
        int line = 0
    );

    // Non-fatal warning: logged to logs/warning_<timestamp>.log, program continues.
    static void LogWarning(
        const std::string& context,
        const std::string& details,
        const char* file = nullptr,
        int line = 0
    );

    // Generic non-fatal log to a custom file inside logs/.
    static void LogToFile(
        const std::string& filename,
        const std::string& level,
        const std::string& context,
        const std::string& details,
        const char* file = nullptr,
        int line = 0
    );
};

#define FATAL_LOG(context, details) \
    CrashLogger::LogFatal(context, details, __FILE__, __LINE__)

#define FATAL_CHECK(condition, context, details) \
    do { if (!(condition)) CrashLogger::LogFatal(context, details, __FILE__, __LINE__); } while (0)

#define FATAL_CHECK_RESULT(result, context) \
    do { if ((result).IsErr()) CrashLogger::LogFatal(context, (result).Error(), __FILE__, __LINE__); } while (0)

#define ERROR_LOG(context, details) \
    CrashLogger::LogError(context, details, __FILE__, __LINE__)

#define WARNING_LOG(context, details) \
    CrashLogger::LogWarning(context, details, __FILE__, __LINE__)

#define LOG_TO_FILE(filename, level, context, details) \
    CrashLogger::LogToFile(filename, level, context, details, __FILE__, __LINE__)
