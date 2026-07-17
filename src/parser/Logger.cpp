#include "Logger.h"


namespace skin_parser {

    Logger& Logger::instance() {
        static Logger instance;
        return instance;
    }

    Logger::Logger() {
        start_time_ = std::chrono::steady_clock::now();
        log_file_.open("skin_parser.log");
        log_file_ << "Dota 2 Skin Parser Log" << std::endl;
        log_file_ << "Started at: " << getCurrentTime() << std::endl;
        log_file_ << "==========================================" << std::endl;
    }

    Logger::~Logger() {
        if (log_file_.is_open()) {
            log_file_ << "==========================================" << std::endl;
            log_file_ << "Finished at: " << getCurrentTime() << std::endl;
            log_file_.close();
        }
    }

    std::string Logger::getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string Logger::getElapsedTime() {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        return std::to_string(seconds) + "s";
    }

    void Logger::log(const std::string& message, bool console) {
        std::string log_entry = "[" + getCurrentTime() + " | +" + getElapsedTime() + "] " + message;
        if (console) {
            std::cout << log_entry << std::endl;
        }
        log_file_ << log_entry << std::endl;
        if (callback_) {
            callback_(log_entry);
        }
    }

    void Logger::logSection(const std::string& section_name) {
        std::string separator = "=================== " + section_name + " ===================";
        log(separator);
    }

    void Logger::logStats(const std::string& category, int count, int total) {
        std::string message = category + ": " + std::to_string(count);
        if (total != -1) {
            int percentage = (total > 0) ? (count * 100) / total : 0;
            message += " / " + std::to_string(total) + " (" + std::to_string(percentage) + "%)";
        }
        log(message);
    }

} // namespace skin_parser