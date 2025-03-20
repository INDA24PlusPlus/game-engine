#ifndef _GAME_H
#define _GAME_H

#include "engine/arena.h"
#include "engine/renderer.h"
#include "engine/platform.h"
#include "engine/camera.h"
#include "engine/keyboard.h"

namespace game {

struct State {
    renderer::Context* renderer_context;
    platform::WindowInfo window_info;

    engine::Camera camera;
    engine::Keyboard kb;

    f32 delta_time;
    f32 curr_time;
};

void start(State& state, platform::WindowInfo window_info);
void shutdown(State& state);
b32 update(State& state, f32 delta_time);
}

#endif
