#include "CrashLogger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdlib.h>
#include "SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_CrashLogger() noexcept {
    if (SH_PebBeingDebugged()) g_integritySeed ^= 0xAABBCCDD;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace fs = std::filesystem;

static std::mutex& GetCrashLoggerMutex() {
    SH_AD_CrashLogger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
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
            fs::create_directories(OBF_CSTR("logs"));
        }
        catch (...) {
        }

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &time_t_now);

        std::ofstream log(filename, std::ios::app);
        if (log.is_open()) {
            log << OBF_CSTR("[") << level << OBF_CSTR("] ")
                << std::put_time(&local_tm, OBF_CSTR("%Y-%m-%d %H:%M:%S")) << OBF_CSTR("\n");
            log << OBF_CSTR("Context: ") << context << OBF_CSTR("\n");
            if (file && line > 0) {
                log << OBF_CSTR("Location: ") << file << OBF_CSTR(":") << line << OBF_CSTR("\n");
            }
            log << OBF_CSTR("Details: ") << details << OBF_CSTR("\n");
            log << OBF_CSTR("----------------------------------------\n");
            log.flush();
        }
    }

    std::string BuildLogFilename(const char* prefix) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
        localtime_s(&local_tm, &time_t_now);

        std::ostringstream filename;
        filename << OBF_CSTR("logs/") << prefix
            << std::put_time(&local_tm, OBF_CSTR("%Y-%m-%d_%H-%M-%S"))
            << OBF_CSTR(".log");
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
    SH_AD_CrashLogger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("error_"), OBF_CSTR("FATAL"), context, details, file, line);

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
    SH_AD_CrashLogger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("error_"), OBF_CSTR("ERROR"), context, details, file, line);
}

void CrashLogger::LogWarning(
    const std::string& context,
    const std::string& details,
    const char* file,
    int line
) {
    SH_AD_CrashLogger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(BuildLogFilename("warning_"), OBF_CSTR("WARNING"), context, details, file, line);
}

void CrashLogger::LogToFile(
    const std::string& filename,
    const std::string& level,
    const std::string& context,
    const std::string& details,
    const char* file,
    int line
) {
    SH_AD_CrashLogger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(GetCrashLoggerMutex());

    WriteLogEntry(OBF_CSTR("logs/") + filename, level, context, details, file, line);
}
