#ifndef _RENDERER_H
#define _RENDERER_H

#include "arena.h"
#include "platform.h"

#include <glm/glm.hpp>

namespace renderer {

struct Context;

enum class PresentState {
    optimal,
    suboptimal,
};

[[nodiscard]] Context* initialize(const char* app_name, u32 fb_width, u32 fb_height, platform::WindowInfo window_info);
void destroy_context(Context* context);
void resize_swapchain(Context* context, u32 width, u32 height);
PresentState present(Context* context, f32 delta_time, glm::mat4 view_matrix);

// TEMP
void create_state_for_triangle(Context* context);

};

#endif
