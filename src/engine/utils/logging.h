#ifndef _LOGGING_H
#define _LOGGING_H

#ifdef DEBUG
    #pragma push_macro("DEBUG")
    #undef DEBUG
#endif

#include <print>
#include <chrono>

#define ANSI_START "\033["
#define ANSI_BOLD ANSI_START "1m"
#define ANSI_RESET "\033[0m"

#ifndef LOG_LEVELS_LIST
#define LOG_LEVELS_LIST(f) \
    f(LOG_LEVEL_ERROR, "Error", 230, 80, 70) \
    f(LOG_LEVEL_WARN, "Warn", 222, 172, 22) \
    f(LOG_LEVEL_INFO, "Info", 200, 200, 200) \
    f(LOG_LEVEL_DEBUG, "Debug", 160, 160, 160)

#define GET_LOG_LEVEL_ENUM_NAME(NAME, ...) NAME,
enum log_levels_t {
    LOG_LEVELS_LIST(GET_LOG_LEVEL_ENUM_NAME)
};

#define GENERATE_LOG_LEVEL_TO_STR(LEVEL, STR, ...) case LEVEL: return STR;
static inline const char * get_log_level_str(enum log_levels_t level) { \
    switch (level) { \
        LOG_LEVELS_LIST(GENERATE_LOG_LEVEL_TO_STR) \
        default: return "UNDEFINED"; \
    } \
}

struct _log_level_rgb {
    int R, G, B;
};

#define GENERATE_LOG_LEVEL_RGB(LEVEL, STR, _R, _G, _B) case LEVEL: return (_log_level_rgb) {.R=_R, .G=_G, .B=_B};
static inline const struct _log_level_rgb get_log_level_rgb(enum log_levels_t level) { \
        switch (level) { \
            LOG_LEVELS_LIST(GENERATE_LOG_LEVEL_RGB) \
            default: return (_log_level_rgb) {.R=0, .G=0, .B=0}; \
        } \
    }

#endif

///* Windows-specific timing functions */
//#ifdef _WIN32
//#include <windows.h>
//static inline int get_milliseconds() {
//    SYSTEMTIME st;
//    GetLocalTime(&st);
//    return st.wMilliseconds;
//}
//#else
//#include <sys/time.h>
//static inline int get_milliseconds() {
//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    return tv.tv_usec / 1000;
//}
//#endif

#define CSTR_LEN(str) (sizeof(str) - 1)

//static inline struct tm get_time() {
//    time_t time_now;
//    struct tm time_info;
//
//    time(&time_now);
//#ifdef _WIN32
//    localtime_s(&time_info, &time_now);
//#else
//    localtime_r(&time_now, &time_info);
//#endif
//
//    return time_info;
//}
//
#define TO_STR(x) #x
#define TO_STR_VALUE(x) TO_STR(x)
//
//#define LOGGER_LOG(level, FMT, ...) { \
//    struct tm time_info = get_time(); \
//    int ms = get_milliseconds(); \
//    auto color = get_log_level_rgb(level); \
//    std::print("[" ANSI_BOLD "{:02}:{:02}:{:02}.{:03}" ANSI_RESET "]" ANSI_START "38:2:{}:{}:{}m [" __FILE_NAME__ ":" TO_STR_VALUE(__LINE__) "]: " FMT ANSI_RESET "\n", time_info.tm_hour, time_info.tm_min, time_info.tm_sec, ms, color.R, color.G, color.B, ##__VA_ARGS__); \
//}

#define LOGGER_LOG(level, FMT, ...) { \
    auto now = std::chrono::system_clock::now(); \
    auto local = std::chrono::zoned_time{std::chrono::current_zone(), now}; \
    auto color = get_log_level_rgb(level); \
    std::print("[" ANSI_BOLD "{:%H:%M:%S}" ANSI_RESET "]" ANSI_START "38:2:{}:{}:{}m [" __FILE_NAME__ ":" TO_STR_VALUE(__LINE__) "]: " FMT ANSI_RESET "\n", local, color.R, color.G, color.B, ##__VA_ARGS__);  \
}


#define INFO(fmt, ...) LOGGER_LOG(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define DEBUG(fmt, ...) LOGGER_LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) LOGGER_LOG(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) LOGGER_LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define FATAL(fmt, ...) ERROR(fmt, ##__VA_ARGS__); exit(1)

#ifdef DEBUG
    #pragma pop_macro("DEBUG")
#endif

#endif
