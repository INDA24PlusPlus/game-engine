#include "game.h"

#include <print>

namespace game {

void start(State& state, platform::WindowInfo window_info) {
    u32 width;
    u32 height;
    platform::get_framebuffer_size(window_info, &width, &height);

    state.window_info = window_info;
    state.curr_time = 0;
    state.camera = engine::Camera::init(glm::vec3(0.0f), 10.0, glm::radians(90.0f));
    state.kb.reset();

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
                state.kb.register_key(event.key_event);
            } break;
            case platform::OSEvent::Kind::key_up: {
                state.kb.unregister_key(event.key_event);
            } break;
            default: {}
        }
    }

    state.delta_time = delta_time;
    state.curr_time += delta_time;

    if (state.kb.is_key_down('W')) {
        state.camera.move(engine::Camera::Direction::forward, delta_time);
    }
    if (state.kb.is_key_down('S')) {
        state.camera.move(engine::Camera::Direction::backward, delta_time);
    }
    if (state.kb.is_key_down('D')) {
        state.camera.move(engine::Camera::Direction::right, delta_time);
    }
    if (state.kb.is_key_down('A')) {
        state.camera.move(engine::Camera::Direction::left, delta_time);
    }
    if (state.kb.is_key_down('Q')) {
        state.camera.move(engine::Camera::Direction::upward, delta_time);
    }
    if (state.kb.is_key_down('E')) {
        state.camera.move(engine::Camera::Direction::downward, delta_time);
    }

    u32 width;
    u32 height;
    platform::get_framebuffer_size(state.window_info, &width, &height);

    // Don't present if the window is minimized
    if (width != 0 && height != 0) {
        auto present_state = renderer::present(state.renderer_context, state.curr_time, state.camera.view_matrix());
        if (present_state == renderer::PresentState::suboptimal) {
            renderer::resize_swapchain(state.renderer_context, width, height);
        }
    }

    return 1;
}

}
