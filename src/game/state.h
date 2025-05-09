#include <array>

#include "backends/imgui_impl_glfw.h"
#include "engine/Camera.h"
#include "engine/Renderer.h"
#include "engine/ecs/resource.hpp"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "game/player.h"
#include "game/enemy.h"
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
