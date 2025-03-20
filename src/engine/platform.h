#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <string>
#include <string_view>
#include "core.h"

namespace platform {

struct WindowInfo {
    void* window_handle;
    void* surface_handle;
};

struct OSHandle {
    void* handle;
};

struct KeyEvent {
    u8 virtual_key;
};

struct OSEvent {
    enum class Kind {
        user_quit_request,
        key_down,
        key_up,
        none,
    };
    Kind kind;
    union {
        KeyEvent key_event;
    };
};


b32 next_os_event(WindowInfo info, OSEvent* e);

// TODO: Better error reporting, Add user specified flags (Read, Write, Execute)./
[[nodiscard]] OSHandle open_file(const char* path);
void close_file(OSHandle handle);

[[nodiscard]] u64 get_file_size(OSHandle file);

/// Attempts to read a number of bytes equal to the length of out buffer slice.
/// Returns the number of bytes read.
[[nodiscard]] u64 read_from_file(OSHandle file, Slice<u8> out_buffer); 

void get_framebuffer_size(WindowInfo window_info, u32* width, u32* height);

[[noreturn]] void fatal(std::string_view msg);

/// Requests and commits memory pages from the OS.
/// Works on page granularity.
[[nodiscard]] void* allocate(size_t length);

/// Frees memory allocated by platform::allocate
void free(void* addr, size_t length);

}

#endif
