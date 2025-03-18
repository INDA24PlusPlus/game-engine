#include <print>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform.h"
#include "game.h"

static std::string get_last_error() {
    auto error_code = GetLastError();
    if (error_code == 0) {
        return "No WIN32 error";
    }

    LPSTR message_buf = nullptr;

    auto size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message_buf,
        0,
        NULL
    );

    std::string message = "Unknown error";
    if (size > 0) {
        message = std::string(message_buf, size);
        LocalFree(message_buf);
    }

    return message;
}

LRESULT WINAPI window_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
    }

    return DefWindowProcW(window, msg, w_param, l_param);
}


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, PWSTR cmd_line, int show_code) {
    (void)prev_instance;
    (void)cmd_line;
    (void)show_code;

    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        .lpszClassName = L"vulkan_rendere_window_class",
    };

    if (RegisterClassExW(&window_class) == 0) {
        platform::fatal(std::format("Failed to register window class with error: {}", get_last_error()));
    }

    auto window = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"Vulkan Renderer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr,
        instance,
        nullptr
    );

    if (window == nullptr) {
        platform::fatal(std::format("Failed to create window class with error: {}", get_last_error()));
    }

    platform::WindowInfo window_info = {
        .window_handle = instance,
        .surface_handle = window,
    };

    u64 timer_frequency;
    QueryPerformanceFrequency((LARGE_INTEGER*)&timer_frequency);
    std::println("Timer frequency is {}Hz", timer_frequency);

    game::State game_state;
    game::start(game_state, window_info);

    ShowWindow(window, SW_SHOW);

    u64 timer_now;
    QueryPerformanceCounter((LARGE_INTEGER*)&timer_now);
    u64 timer_prev = timer_now;

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        QueryPerformanceCounter((LARGE_INTEGER*)&timer_now);
        u64 delta_ticks = timer_now - timer_prev;
        timer_prev = timer_now;

        f32 delta_time = (f32)delta_ticks / (f32)timer_frequency;
        game::update(game_state, delta_time);
    }

    game::shutdown(game_state);
}


int main(void) {
    // FIXME: Incorrect handling of command line arguments.
    return wWinMain(GetModuleHandleW(nullptr), nullptr, nullptr, SW_SHOW);
}
