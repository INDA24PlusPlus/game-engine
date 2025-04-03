#include <print>

#include <windows.h>
#include "platform.h"

// TEMP
#include "game/game.h"

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
    auto e = (platform::OSEvent*)GetWindowLongPtr(window, GWLP_USERDATA);

    // FIXME: I do not like this..
    // Windows sends us messages before we get to set the user pointer.
    if (e != nullptr) {
        switch (msg) {
            case WM_DESTROY: {
                e->kind = platform::OSEvent::Kind::user_quit_request;
                PostQuitMessage(0);
                break;
            }
            // FIXME: Make this more robust.
            case WM_INPUT: {
                u32 dwSize = sizeof(RAWINPUT);
                static u8 lpb[sizeof(RAWINPUT)];

                GetRawInputData((HRAWINPUT)l_param, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
                RAWINPUT* raw = (RAWINPUT*)lpb;

                if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                    const RAWKEYBOARD& kb = raw->data.keyboard;
                    u32 vkey = kb.VKey;
                    if (vkey > 0 && vkey <= 255) {
                        e->key_event.virtual_key = vkey;
                        bool is_key_down = !(kb.Flags & RI_KEY_BREAK);

                        if (is_key_down) {
                            e->kind = platform::OSEvent::Kind::key_down;
                        } else {
                            e->kind = platform::OSEvent::Kind::key_up;
                        }
                    }
                } else if (raw->header.dwType == RIM_TYPEMOUSE) {
                }

                break;
            }
        }
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

    // Register raw input events.
    RAWINPUTDEVICE Rid[2];
    Rid[0] = {
        .usUsagePage = 0x01,      // HID_USAGE_PAGE_GENERIC
        .usUsage = 0x06,          // HID_USAGE_GENERIC_KEYBOARD
        .dwFlags = 0,
        .hwndTarget = window,
    };
    Rid[1] = {
        .usUsagePage = 0x01,      // HID_USAGE_PAGE_GENERIC
        .usUsage = 0x02,          // HID_USAGE_GENERIC_MOUSE
        .dwFlags = 0,
        .hwndTarget = window,
    };

    if (RegisterRawInputDevices(Rid, 2, sizeof(RAWINPUTDEVICE)) == FALSE) { platform::fatal("Failed to register raw input device for user keyboard and mouse. TODO: Fallback to using legacy keyboard and mouse messages");
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

    while (true) {
        QueryPerformanceCounter((LARGE_INTEGER*)&timer_now);
        u64 delta_ticks = timer_now - timer_prev;
        timer_prev = timer_now;

        f32 delta_time = (f32)delta_ticks / (f32)timer_frequency;
        if (!game::update(game_state, delta_time)) {
            break;
        }
    }

    game::shutdown(game_state);
}


int main(void) {
    // FIXME: Incorrect handling of command line arguments.
    return wWinMain(GetModuleHandleW(nullptr), nullptr, nullptr, SW_SHOW);
}
