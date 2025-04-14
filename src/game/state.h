#include "engine/Camera.h"
#include "engine/Scene.h"

struct State {
    engine::Scene scene;
    engine::Camera camera;
    bool mouse_locked;

    f32 prev_time;
    f32 curr_time;
    f32 delta_time;
    f32 sensitivity;
};