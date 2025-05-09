#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include "game/settings.h"
#include "glm/fwd.hpp"

class RInput : public Resource<RInput> {
public:
  engine::Input input;
  Settings settings;
  RInput(GLFWwindow *window) : input(window) {}

  // returns a vec3 with xz coordinates for movement keys in settings
  glm::vec3 get_direction() {
    glm::vec3 direction(0.0f);

    if (input.is_key_pressed(settings.key_forward))
      direction.z += 1.0f;
    if (input.is_key_pressed(settings.key_backward))
      direction.z -= 1.0f;
    if (input.is_key_pressed(settings.key_left))
      direction.x -= 1.0f;
    if (input.is_key_pressed(settings.key_right))
      direction.x += 1.0f;

    return direction;
  }
};

class RRenderer : public Resource<RRenderer> {
public:
  engine::Renderer m_renderer;
  RRenderer(engine::Renderer renderer) : m_renderer(renderer) {}
};

class SInput : public System<SInput> {
public:
  SInput();
  void update(ECS &ecs);
};

class SUIRender : public System<SUIRender> {
public:
  SUIRender();
  void update(ECS &ecs);
};

class SRender : public System<SRender> {
public:
  SRender();
  void update(ECS &ecs);
};

class SCamera : public System<SCamera> {
public:
  SCamera();
  void update(ECS &ecs);
};
