#ifndef _LOGGING_H
#define _LOGGING_H

#ifdef DEBUG
    #pragma push_macro("DEBUG")
    #undef DEBUG
#endif

#include <cstdio>
#include <ctime>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <array>

#define ANSI_START "\033["
#define ANSI_RESET "\033[0m"

enum class LogLevels {
    ERROR,
    WARN,
    INFO,
    DEBUG,

    // Used to get enum member count
    COUNT
};

struct _LOG_RGB {
    int R, G, B;
};

template <typename EnumType, std::size_t N>
struct log_level_info {
    //                              LogLevel String    LogLevel RGB color
    std::array<std::tuple<EnumType, std::string_view, _LOG_RGB>, N> mappings;

    [[nodiscard]]
    constexpr std::string_view to_string(EnumType value) const {
        for (const auto &[enum_val, str, _] : mappings) {
            if (enum_val == value) {
                return str;
            }
        }

        return "UNDEFINED";
    }

    [[nodiscard]]
    constexpr _LOG_RGB get_color(EnumType value) const {
        for (const auto &[enum_val, _, color] : mappings) {
            if (enum_val == value) {
                return color;
            }
        }

        return _LOG_RGB(0, 0, 0);
    }
};

constexpr auto log_level_list = log_level_info<LogLevels, static_cast<size_t>(LogLevels::COUNT)>{{{
    {LogLevels::ERROR, "Error", _LOG_RGB(230, 80, 70)},
    {LogLevels::WARN, "Warn", _LOG_RGB(222, 172, 22)},
    {LogLevels::INFO, "Info", _LOG_RGB(200, 200, 200)},
    {LogLevels::DEBUG, "Debug", _LOG_RGB(160, 160, 160)},
}}};

class Logger {
   public:
    static std::string time(bool include_ms = true);
    static std::string color(int R, int G, int B, bool bold = true);

    template <LogLevels level, typename... Args>
    static void log_with_position(const std::string file_name_str, const int line_number,
                                  std::format_string<Args...> fmt, Args &&...args);

   private:
    static std::ostringstream &time_stream(std::ostringstream &stream, bool include_ms = true);
    static std::ostringstream &color_stream(std::ostringstream &stream, int R, int G, int B,
                                            bool bold = true);
};

template <LogLevels level, typename... Args>
void Logger::log_with_position(const std::string file_name_str, const int line_number,
                               std::format_string<Args...> fmt, Args &&...args) {
    std::ostringstream stream;

    auto color = log_level_list.get_color(level);
    stream << "[";
    Logger::time_stream(stream) << "]";
    Logger::color_stream(stream, color.R, color.G, color.B)
        << " [" << file_name_str << ":" << line_number << "] " << log_level_list.to_string(level)
        << ": ";

    std::cout << stream.str() << std::format(fmt, std::forward<Args>(args)...) << ANSI_RESET
              << std::endl;
}

#define LOGGER_LOG(level, fmt, ...) \
    Logger::log_with_position<level>(__FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) LOGGER_LOG(LogLevels::INFO, fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) LOGGER_LOG(LogLevels::ERROR, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) LOGGER_LOG(LogLevels::WARN, fmt, ##__VA_ARGS__)
#define DEBUG(fmt, ...) LOGGER_LOG(LogLevels::DEBUG, fmt, ##__VA_ARGS__)

#ifdef DEBUG
    #pragma pop_macro("DEBUG")
#endif

#endif