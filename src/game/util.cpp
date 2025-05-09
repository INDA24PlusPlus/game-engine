#include "util.h"
#include "components.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include "game/settings.h"
#include "glm/fwd.hpp"
#include "state.h"
#include "time.h"
#include <cstring>

SInput::SInput() {}
void SInput::update(ECS &ecs) {
  auto input = ecs.get_resource<RInput>();
  input->input.update();
}

SUIRender::SUIRender() {}

inline glm::vec4 from_hex(const char *h) {
  unsigned int r, g, b, a = 255;
  if (std::strlen(h) == 7)
    std::sscanf(h, "#%02x%02x%02x", &r, &g, &b);
  else
    std::sscanf(h, "#%02x%02x%02x%02x", &r, &g, &b, &a);
  return glm::vec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}

void SUIRender::update(ECS &ecs) {
  auto time = ecs.get_resource<RDeltaTime>();
  auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
  auto scene = ecs.get_resource<RScene>();
  auto window = scene->m_window;

  int height;
  int width;
  glfwGetFramebufferSize(window, &width, &height);

  // DRAW HEALTH BAR

  // TODO: Get nearest player instead of only player in single player
  Entity &nearest_player = scene->m_player;
  CHealth &nearest_player_health = ecs.get_component<CHealth>(nearest_player);

  // Draw
  renderer->begin_rect_pass();
  {
    float hpbar_width = 256.f;
    float hpbar_height = 32.f;
    float margin = 12.f;
    float border_size = 4.f;

    static float smooth_health = nearest_player_health.health;
    static float last_health = nearest_player_health.health;
    static float damage_timer = 0.f;

    // smooth taken damage indicator bar
    if (nearest_player_health.health < last_health) {
      last_health = nearest_player_health.health;
      damage_timer = 0.2f;
    }
    if (damage_timer > 0.f) {
      damage_timer -= time->delta_time;
      if (damage_timer <= 0.f) {
        last_health = nearest_player_health.health;
      }
    } else {
      smooth_health = glm::mix(smooth_health, nearest_player_health.health,
                               time->delta_time * 10.f);
    }

    float max_health = 100.f; // temporary

    // damage indicator bar (shows how much damage was taken)
    float damage_bar_width =
        hpbar_width * (nearest_player_health.health / max_health);
    renderer->draw_rect({.x = (float)width - hpbar_width - margin,
                         .y = margin,
                         .width = damage_bar_width,
                         .height = hpbar_height},
                        from_hex("#ff0000a0"));

    // hp bar
    float bar_width = hpbar_width * (smooth_health / max_health);
    renderer->draw_rect({.x = (float)width - hpbar_width - margin,
                         .y = margin,
                         .width = bar_width,
                         .height = hpbar_height},
                        from_hex("#ff8519a0"));

    // background
    renderer->draw_rect({.x = (float)width - hpbar_width - margin,
                         .y = margin,
                         .width = hpbar_width,
                         .height = hpbar_height},
                        from_hex("#000000a0"));

    // border
    renderer->draw_rect({.x = (float)width - hpbar_width - margin - border_size,
                         .y = margin - border_size,
                         .width = hpbar_width + 2 * border_size,
                         .height = hpbar_height + 2 * border_size},
                        from_hex("#2a2a2aff"));
  }
  renderer->end_rect_pass(width, height);
}

SRender::SRender() {
  queries[0] = Query(CTranslation::get_id(), CMesh::get_id());
  query_count = 1;
}

void SRender::update(ECS &ecs) {
  auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
  auto scene = ecs.get_resource<RScene>();
  auto hierarchy = scene->m_hierarchy;
  auto camera = scene->m_camera;
  auto window = scene->m_window;
  int height;
  int width;

  auto entities = get_query(0)->get_entities();
  Iterator it = {.next = 0};
  Entity e;
  while (entities->next(it, e)) {
    auto translation = ecs.get_component<CTranslation>(e);
    auto mesh = ecs.get_component<CMesh>(e);
    hierarchy.m_nodes[mesh.m_node.get_value()].translation = translation.pos;
    hierarchy.m_nodes[mesh.m_node.get_value()].rotation = translation.rot;
    hierarchy.m_nodes[mesh.m_node.get_value()].scale = translation.scale;
  }

  glfwPollEvents();
  glfwGetFramebufferSize(window, &width, &height);

  // Draw
  renderer->clear();
  renderer->begin_pass(scene->m_scene, camera, width, height);
  renderer->draw_hierarchy(scene->m_scene, hierarchy);
  renderer->end_pass();
}

SCamera::SCamera() {
  queries[0] =
      Query(CTranslation::get_id(), CLocal::get_id(), CPlayer::get_id());
  query_count = 1;
}

void SCamera::update(ECS &ecs) {
  float delta_time = ecs.get_resource<RDeltaTime>()->delta_time;
  auto scene = ecs.get_resource<RScene>();
  auto window = scene->m_window;
  RInput *input = ecs.get_resource<RInput>();
  ;

  engine::Camera &camera = scene->m_camera;

  // mouse locking
  bool mouse_locked =
      glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
  if (input->input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
    mouse_locked = !mouse_locked;
    glfwSetInputMode(window, GLFW_CURSOR,
                     mouse_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    INFO("debbbbiiiiig");
  }

  // camera rotation mouse input
  if (mouse_locked) {
    glm::vec2 mouse_delta = input->input.get_mouse_position_delta();
    // INFO("debbbb x: {} y: {}", mouse_delta.x, mouse_delta.y);
    camera.rotate(-mouse_delta.x * 0.00048f * input->settings.sensitivity,
                  -mouse_delta.y * 0.00048f * input->settings.sensitivity);
  }

  // camera movement
  if (input->input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
    // free cam movement
    // movement input
    glm::vec3 direction = input->get_direction();

    if (glm::dot(direction, direction) > 0) {
      camera.move(glm::normalize(direction), delta_time);
    }
  } else { // this query could be changed to something like CCameraFollow
           // instead of being hardcoded to local players
    // follow player cam
    // get players
    auto players = get_query(0)->get_entities();
    glm::vec3 mean_player_position = glm::vec3(0.0f, 0.0f, 0.0f);
    Iterator it = {.next = 0};
    Entity e;
    int player_count = 0;
    while (players->next(it, e)) {
      mean_player_position += ecs.get_component<CTranslation>(e).pos;
      player_count++;
    }
    mean_player_position /= player_count;

    if (player_count > 0) {
      float camera_distance = 10;

      glm::vec3 camera_target_position =
          mean_player_position -
          camera.m_orientation * glm::vec3(0, 0, -1) * camera_distance;
      camera.m_pos =
          glm::mix(camera.m_pos, camera_target_position, delta_time * 5);
    }
  }
}
