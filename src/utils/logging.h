#include <cstdio>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <chrono>
#include <string>
#include <string_view>

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

template<typename EnumType, std::size_t N>
struct log_level_info {
    //                              LogLevel String    LogLevel RGB color
    std::array<std::tuple<EnumType, std::string_view, _LOG_RGB>, N> mappings;

    [[nodiscard]]
    constexpr std::string_view to_string(EnumType value) const {
        for (const auto& [enum_val, str, _] : mappings) {
            if (enum_val == value) {
                return str;
            }
        }

        return "UNDEFINED";
    }

    [[nodiscard]]
    constexpr _LOG_RGB get_color(EnumType value) const {
        for (const auto& [enum_val, _, color] : mappings) {
            if (enum_val == value) {
                return color;
            }
        }

        return _LOG_RGB(0, 0, 0);
    }
};

constexpr auto log_level_list = log_level_info<LogLevels, static_cast<size_t>(LogLevels::COUNT)>{{{
    {LogLevels::ERROR, "Error", _LOG_RGB(230,  80,  70)},
    {LogLevels::WARN,  "Warn",  _LOG_RGB(222, 172,  22)},
    {LogLevels::INFO,  "Info",  _LOG_RGB(200, 200, 200)},
    {LogLevels::DEBUG, "Debug", _LOG_RGB(160, 160, 160)},
}}};

class Logger {
    public:
        Logger(const std::string name) : ref(name) {};

        static std::string time(bool include_ms = true);
        static std::string color(int R, int G, int B, bool bold = true);

        template<LogLevels Level = LogLevels::INFO, typename... Args>
        void log(std::format_string<Args...>, Args&&... args);

        template<LogLevels level, typename... Args>
        void log_with_position(const std::string file_name_str, const int line_number, std::format_string<Args...> fmt, Args&&... args);
    public:
        const std::string ref;
        static LogLevels log_level;
    private:
        Logger(const Logger&);
        Logger& operator =(const Logger&);

        static std::ostringstream& time_stream(std::ostringstream& stream, bool include_ms = true);
        static std::ostringstream& color_stream(std::ostringstream& stream, int R, int G, int B, bool bold = true);
};

template<LogLevels level, typename... Args>
void Logger::log(std::format_string<Args...> fmt, Args&&... args) {
    std::ostringstream stream;

    auto color = log_level_list.get_color(level);
    stream << "[";
    Logger::time_stream(stream) << "] [" << ref << "] ";
    Logger::color_stream(stream, color.R, color.G, color.B) << log_level_list.to_string(level) << ": ";
    
    std::cout << stream.str() << std::format(fmt, std::forward<Args>(args)...) << ANSI_RESET << std::endl;
}

template<LogLevels level, typename... Args>
void Logger::log_with_position(const std::string file_name_str, const int line_number, std::format_string<Args...> fmt, Args&&... args) {
    std::ostringstream stream;

    auto color = log_level_list.get_color(level);
    stream << "[";
    Logger::time_stream(stream) << "]";
    Logger::color_stream(stream, color.R, color.G, color.B) << " [" << file_name_str << ":" << line_number  << "] " << log_level_list.to_string(level) << ": ";
    
    std::cout << stream.str() << std::format(fmt, std::forward<Args>(args)...) << ANSI_RESET << std::endl;
}

#define LOGGER_LOG(logger, level, fmt, ...) logger.log_with_position<level>(__FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGGER_INFO(logger, fmt, ...) LOGGER_LOG(logger, LogLevels::INFO, fmt, ##__VA_ARGS__)
#define LOGGER_ERROR(logger, fmt, ...) LOGGER_LOG(logger, LogLevels::ERROR, fmt, ##__VA_ARGS__)
#define LOGGER_WARN(logger, fmt, ...) LOGGER_LOG(logger, LogLevels::WARN, fmt, ##__VA_ARGS__)
#define LOGGER_DEBUG(logger, fmt, ...) LOGGER_LOG(logger, LogLevels::DEBUG, fmt, ##__VA_ARGS__)

