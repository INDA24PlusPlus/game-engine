#include <array>

#include "backends/imgui_impl_glfw.h"
#include "engine/Camera.h"
#include "engine/Renderer.h"
#include "engine/ecs/resource.hpp"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "game/player.h"
#include "game/enemy.h"

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
    Enemy enemy;
    Enemy enemy2;
};

class RScene : public Resource<RScene> {
public:
  engine::Scene m_scene;
  engine::Camera m_camera;
  engine::NodeHierarchy m_hierarchy;
  GLFWwindow *m_window;
  Entity m_player; // temporary way of getting the player in singleplayer
  RScene(engine::Scene scene, engine::Camera camera, GLFWwindow *window,
         engine::NodeHierarchy hierarchy, Entity player)
      : m_scene(scene), m_camera(camera), m_hierarchy(hierarchy),
        m_window(window), m_player(player) {}
};
