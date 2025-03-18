#include "game.h"

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

void update(State& state, f32 delta_time) {
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
}

}
