#ifndef _GUI_H
#define _GUI_H


struct State;

#include <GLFW/glfw3.h>
#include "engine/core.h"

namespace gui {
    void init(GLFWwindow* window, f32 content_scale);
    void deinit();
    void build(State& state);
    void render();
}

static void draw_node(State& state, u32 node_index);

#endif
