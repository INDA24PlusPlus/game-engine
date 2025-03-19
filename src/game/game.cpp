#include "game.h"

#include <print>

namespace game {

void start(State& state, platform::WindowInfo window_info) {
    u32 width;
    u32 height;
    platform::get_framebuffer_size(window_info, &width, &height);

    state.curr_time = 0;
    state.window_info = window_info;
    state.renderer_context = renderer::initialize("Vulkan renderer", width, height, window_info);
    renderer::create_state_for_triangle(state.renderer_context);
}

void shutdown(State& state) {
    renderer::destroy_context(state.renderer_context);
}

b32 update(State& state, f32 delta_time) {
    // Process events.
    platform::OSEvent event;
    while (platform::next_os_event(state.window_info, &event)) {
        switch (event.kind) {
            case platform::OSEvent::Kind::user_quit_request: {
                return 0;
            } break;
            case platform::OSEvent::Kind::key_down: {
                std::println("here");
            } break;
            case platform::OSEvent::Kind::key_up: {
            } break;
            default: {}
        }
    }

    state.delta_time = delta_time;
    state.curr_time += delta_time;

    u32 width;
    u32 height;
    platform::get_framebuffer_size(state.window_info, &width, &height);

    // Don't present if the window is minimized
    if (width != 0 && height != 0) {
        auto present_state = renderer::present(state.renderer_context, state.curr_time);
        if (present_state == renderer::PresentState::suboptimal) {
            renderer::resize_swapchain(state.renderer_context, width, height);
        }
    }

    return 1;
}

}
