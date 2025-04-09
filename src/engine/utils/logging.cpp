#include "logging.h"

#include <chrono>

#define SEC_IN_MS 1000

// Log::time but return a ostringstream& instead
std::ostringstream &Logger::time_stream(std::ostringstream &stream, bool include_ms) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    stream << std::put_time(std::localtime(&time), "%H:%M:%S");

    if (include_ms) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
                  SEC_IN_MS;
        stream << "." << ms.count();
    }

    return stream;
}

// Log::color but return a ostringstream& instead
std::ostringstream &Logger::color_stream(std::ostringstream &stream, int R, int G, int B,
                                         bool bold) {
    stream << ANSI_START "38:2:" << R << ":" << G << ":" << B << "m";
    if (bold) {
        stream << ANSI_START "1m";
    }

    return stream;
}

std::string Logger::time(bool include_ms) {
    std::ostringstream os;
    time_stream(os, include_ms);
    return os.str();
}

std::string Logger::color(int R, int G, int B, bool bold) {
    std::ostringstream os;
    color_stream(os, R, G, B, bold);
    return os.str();
}