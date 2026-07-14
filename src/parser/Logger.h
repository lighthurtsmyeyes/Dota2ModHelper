#pragma once

#include <fstream>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>

namespace skin_parser {

    class Logger {
    public:
        static Logger& instance();                 // синглтон для удобства (можно заменить на передачу ссылки)
        void log(const std::string& message, bool console = true);
        void logSection(const std::string& section_name);
        void logStats(const std::string& category, int count, int total = -1);

        void setCallback(std::function<void(const std::string&)> cb) { callback_ = cb; }
        void clearCallback() { callback_ = nullptr; }

        // Запрет копирования
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

    private:
        Logger();
        ~Logger();

        std::ofstream log_file_;
        std::chrono::steady_clock::time_point start_time_;
        std::function<void(const std::string&)> callback_;

        std::string getCurrentTime();
        std::string getElapsedTime();
    };

} // namespace skin_parser