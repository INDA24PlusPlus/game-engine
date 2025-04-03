#ifndef _RENDERER_H
#define _RENDERER_H

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
PresentState present(Context* context, const glm::mat4& view_matrix, f32 curr_time);
void increment_display_bone(Context* context);

};

#endif
