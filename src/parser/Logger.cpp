#include "Logger.h"
#include "../SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_Logger() noexcept {
    if (SH_NtGlobalFlag()) g_integritySeed ^= 0x11223344;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace skin_parser {

    Logger& Logger::instance() {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        static Logger instance;
        return instance;
    }

    Logger::Logger() {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        start_time_ = std::chrono::steady_clock::now();
        log_file_.open(OBF_CSTR("skin_parser.log"));
        log_file_ << OBF_CSTR("Dota 2 Skin Parser Log") << std::endl;
        log_file_ << OBF_CSTR("Started at: ") << getCurrentTime() << std::endl;
        log_file_ << OBF_CSTR("==========================================") << std::endl;
    }

    Logger::~Logger() {
        if (log_file_.is_open()) {
            log_file_ << OBF_CSTR("==========================================") << std::endl;
            log_file_ << OBF_CSTR("Finished at: ") << getCurrentTime() << std::endl;
            log_file_.close();
        }
    }

    std::string Logger::getCurrentTime() {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), OBF_CSTR("%Y-%m-%d %H:%M:%S"));
        return ss.str();
    }

    std::string Logger::getElapsedTime() {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        return std::to_string(seconds) + OBF_CSTR("s");
    }

    void Logger::log(const std::string& message, bool console) {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string log_entry = OBF_CSTR("[") + getCurrentTime() + OBF_CSTR(" | +") + getElapsedTime() + OBF_CSTR("] ") + message;
        if (console) {
            std::cout << log_entry << std::endl;
        }
        log_file_ << log_entry << std::endl;
        if (callback_) {
            callback_(log_entry);
        }
    }

    void Logger::logSection(const std::string& section_name) {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string separator = OBF_CSTR("=================== ") + section_name + OBF_CSTR(" ===================");
        log(separator);
    }

    void Logger::logStats(const std::string& category, int count, int total) {
    SH_AD_Logger();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string message = category + OBF_CSTR(": ") + std::to_string(count);
        if (total != -1) {
            int percentage = (total > 0) ? (count * 100) / total : 0;
            message += OBF_CSTR(" / ") + std::to_string(total) + OBF_CSTR(" (") + std::to_string(percentage) + OBF_CSTR("%)");
        }
        log(message);
    }

} // namespace skin_parser