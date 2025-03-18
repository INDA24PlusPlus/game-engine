#ifndef _GAME_H
#define _GAME_H

#include "arena.h"
#include "renderer.h"
#include "platform.h"

namespace game {

struct State {
    renderer::Context* renderer_context;
    platform::WindowInfo window_info;

    f32 delta_time;
    f32 curr_time;
};

void start(State& state, platform::WindowInfo window_info);
void shutdown(State& state);
void update(State& state, f32 delta_time);
}

#endif
