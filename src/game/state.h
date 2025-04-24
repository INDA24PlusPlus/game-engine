#include <array>

#include "engine/Camera.h"
#include "engine/Renderer.h"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "game/player.h"

struct State {
    engine::Scene scene;
    engine::Camera camera;
    engine::Renderer renderer;
    engine::NodeHierarchy hierarchy;

    u32 fb_width;
    u32 fb_height;
    bool mouse_locked;

    f32 prev_time;
    f32 curr_time;
    f32 delta_time;

    std::array<f32, 50> prev_delta_times;
    u32 fps_counter_index;

    f32 sensitivity;

    Player player;
    Player enemy;  // well maybe the struct should be something else
};
