#include "CrashLogger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdlib.h>


namespace fs = std::filesystem;

static std::mutex& GetCrashLoggerMutex() {
    static std::mutex m;
    return m;
}

namespace {
    void WriteLogEntry(
        const std::string& filename,
        const std::string& level,
        const std::string& context,
        const std::string& details,
        const char* file,
        int line
    ) {
        try {
            fs::create_directories("logs");
        }
        catch (...) {
        }

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &time_t_now);

        std::ofstream log(filename, std::ios::app);
        if (log.is_open()) {
            log << "[" << level << "] "
                << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "\n";
            log << "Context: " << context << "\n";
            if (file && line > 0) {
                log << "Location: " << file << ":" << line << "\n";
            }
            log << "Details: " << details << "\n";
            log << "----------------------------------------\n";
            log.flush();
        }
    }

    std::string BuildLogFilename(const char* prefix) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &time_t_now);

        std::ostringstream filename;
        filename << "logs/" << prefix
            << std::put_time(&local_tm, "%Y-%m-%d_%H-%M-%S")
            << ".log";
        return filename.str();
    }
}

void CrashLogger::LogFatal(
    const std::string& context,
    const std::string& details,
    const char* file,
    int line,
    bool terminate
) {
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("error_"), "FATAL", context, details, file, line);

    if (terminate) {
        std::abort();
    }
}

void CrashLogger::LogError(
    const std::string& context,
    const std::string& details,
    const char* file,
    int line
) {
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("error_"), "ERROR", context, details, file, line);
}

void CrashLogger::LogWarning(
    const std::string& context,
    const std::string& details,
    const char* file,
    int line
) {
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("warning_"), "WARNING", context, details, file, line);
}

void CrashLogger::LogToFile(
    const std::string& filename,
    const std::string& level,
    const std::string& context,
    const std::string& details,
    const char* file,
    int line
) {
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry("logs/" + filename, level, context, details, file, line);
}
