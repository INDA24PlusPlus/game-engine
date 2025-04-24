#include "engine/Camera.h"
#include "engine/Scene.h"
#include "engine/Renderer.h"
#include "engine/network/client.hpp"

struct State {
    engine::Scene scene;
    engine::Camera camera;
    engine::Renderer renderer;
    bool mouse_locked;

    Client multiplayer_client;

    f32 prev_time;
    f32 curr_time;
    f32 delta_time;
    f32 sensitivity;
};
