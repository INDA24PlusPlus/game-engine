#include "platform.h"

#include <windows.h>
#include "core.h"

namespace platform {


b32 next_os_event(WindowInfo info, OSEvent* e) {
    MSG msg = {};
    e->kind = platform::OSEvent::Kind::none;
    SetWindowLongPtr((HWND)info.surface_handle, GWLP_USERDATA, (LONG_PTR)e);

    // Loop until we have an event we want to pass onto the game.
    while (e->kind == platform::OSEvent::Kind::none) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        return 0;
    }

    return 1;
}

// FIXME: https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
// This is hard, so I will do the bare minimum for now.
void get_framebuffer_size(WindowInfo window_info, u32* width, u32* height) {
    HWND window = (HWND)window_info.surface_handle;

    RECT rect;
    assert(GetClientRect(window, &rect) && "Failed to get client rect for window");

    u32 dpi = GetDpiForWindow(window);
    *width = MulDiv(rect.right - rect.left, dpi, 96);
    *height = MulDiv(rect.bottom - rect.top, dpi, 96);
}


[[nodiscard]] OSHandle open_file(const char* path) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    return {file};
}

void close_file(OSHandle handle) {
    assert(handle.handle != nullptr);
    CloseHandle((HANDLE)handle.handle);
}

[[nodiscard]] u64 get_file_size(OSHandle file) {
    u64 file_size = 0;
    if (!GetFileSizeEx((HANDLE)file.handle, (LARGE_INTEGER*)&file_size)) {
        return 0;
    }
    return file_size;
}

[[nodiscard]] u64 read_from_file(OSHandle file, Slice<u8> out_buffer) {
    u64 chunk_size = MB(10);
    u64 total_bytes_read = 0;

    while (total_bytes_read < out_buffer.len) {
        u64 bytes_to_read = min(chunk_size, out_buffer.len - total_bytes_read);

        DWORD bytes_read;
        BOOL result = ReadFile((HANDLE)file.handle, out_buffer.ptr + total_bytes_read, bytes_to_read, &bytes_read, nullptr);
        if (!result || bytes_read == 0) {
            break;
        }

        total_bytes_read += bytes_read;
    }

    return total_bytes_read;
}

[[noreturn]] void fatal(std::string_view msg) {
    MessageBoxA(nullptr, msg.data(), "Fatal Error", MB_ICONWARNING);
    ExitProcess(1);
}


[[nodiscard]] void* allocate(size_t length) {
    void* mem = VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem == nullptr) {
        fatal("Requested memory from the OS but recieved none. (Ran out of memory?)");
    }

    asan_poison_memory_region(mem, length);
    return mem;
}

void free(void* addr, size_t length) {
    VirtualFree(addr, length, MEM_RELEASE);
    asan_poison_memory_region(addr, length);
}

}
