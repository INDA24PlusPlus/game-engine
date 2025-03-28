#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <chrono>
#include <string>

#include "utils/macro.h"

#define ANSI_START "\033["

#ifndef LOG_LEVEL_LIST
#define LOG_LEVEL_LIST(f) \
    f(ERROR) \
    f(WARN) \
    f(INFO) \
    f(DEBUG)

GENERATE_ENUM(log_level_t, LOG_LEVEL_LIST)
GENERATE_ENUM_TO_STR_FUNC_BODY(log_level_t, LOG_LEVEL_LIST)
#endif


class Log {
    public:
        Log();
        virtual ~Log();
        std::ostringstream& time(bool include_ms = true);
        std::ostringstream& color(int R, int G, int B, bool bold = true);
        std::ostringstream& get(log_level_t level = INFO);
    public:
        static log_level_t log_level;
    private:
        Log(const Log&);
        Log& operator =(const Log&);
    private:
        log_level_t message_level;
        std::ostringstream os;
};

std::ostringstream& Log::time(bool include_ms) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    os << std::put_time(std::localtime(&time), "%H:%M:%S");

    if (include_ms) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        os << "." << ms.count();
    }

    return os;
}

std::ostringstream& Log::get(log_level_t level) {
    os << "- ";
    Log::time() << " " << log_level_t_to_string(level) << ":\t";
    message_level = level;

    return os;
}

std::ostringstream& Log::color(int R, int G, int B, bool bold) {
    os << ANSI_START "38:2:" << R << ":" << G << ":" << B << "m";
    if (bold) {
        os << ANSI_START "1m";
    }

    return os;
}

Log::~Log() {
    if (message_level < log_level) {
        return;
    }

    os << std::endl;
    std::cout << os.str();
}
