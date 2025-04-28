#include "engine/Camera.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

#include "engine/AssetLoader.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/core.h"
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/entity.hpp"
#include "engine/ecs/entityarray.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/signature.hpp"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "engine/utils/logging.h"
#include "glm/ext/quaternion_common.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "gui.h"
#include "state.h"
#include "world_gen/map.h"

class RInput : public Resource<RInput> {
public:
  engine::Input m_input;
  RInput(GLFWwindow *window) : m_input(window) {}
};

class RRenderer : public Resource<RRenderer> {
public:
  engine::Renderer m_renderer;
  RRenderer(engine::Renderer renderer) : m_renderer(renderer) {}
};

class RScene : public Resource<RScene> {
public:
  engine::Scene m_scene;
  engine::Camera m_camera;
  engine::NodeHierarchy m_hierarchy;
  GLFWwindow *m_window;
  RScene(engine::Scene scene, engine::Camera camera, GLFWwindow *window)
      : m_scene(scene), m_camera(camera), m_window(window) {}
};

class CPlayer : public Component<CPlayer> {
public:
};

class CLocal : public Component<CLocal> {
public:
};

class COnline : public Component<COnline> {
public:
};

class CVelocity : public Component<CVelocity> {
public:
  glm::vec3 vel;
};

class CPosition : public Component<CPosition> {
public:
  glm::vec3 pos;
};

class CEnemy : public Component<CEnemy> {
public:
};

class CSize : public Component<CSize> {
public:
  glm::vec3 scale;
};

class CName : public Component<CName> {
public:
  char name[32];
};

class CMesh : public Component<CMesh> {
public:
  engine::NodeHandle m_node;
  CMesh(engine::NodeHandle node) : m_node(node) {}
  CMesh() {}
};

class SMove : public System<SMove> {
public:
  SMove() {
    queries[0] = Query(CPosition::get_id(), CVelocity::get_id());
    query_count = 1;
  }

  void update(ECS &ecs) {
    auto entities = get_query(0)->get_entities();
    Iterator it = {.next = 0};
    Entity e;
    while (entities->next(it, e)) {
      CPosition &pos = ecs.get_component<CPosition>(e);
      CVelocity &vel = ecs.get_component<CVelocity>(e);
      pos.pos += vel.vel;
      vel.vel = glm::vec3(0);
    }
  }
};

class SEnemyMove : public System<SEnemyMove> {
public:
  SEnemyMove() {
    queries[0] =
        Query(CVelocity::get_id(), CPosition::get_id(), CEnemy::get_id());
    queries[1] = Query(CPosition::get_id(), CPlayer::get_id());
    query_count = 2;
  }

  void update(ECS &ecs) {
    auto enemy_entities = get_query(0)->get_entities();
    auto player_entities = get_query(1)->get_entities();
    Iterator e_it = {.next = 0};
    Entity e_e;
    while (enemy_entities->next(e_it, e_e)) {
      CPosition &enemy_pos = ecs.get_component<CPosition>(e_e);
      CVelocity &enemy_vel = ecs.get_component<CVelocity>(e_e);
      // CEnemy& enemy = ecs.get_component<CEnemy>(e_e);

      Iterator p_it = {.next = 0};
      Entity p_e;
      float dx = 0;
      float dy = 0;
      float dist = sqrt(dx * dx + dy * dy);

      while (player_entities->next(p_it, p_e)) {
        CPosition &player_pos = ecs.get_component<CPosition>(p_e);

        float new_dx = player_pos.pos.x - enemy_pos.pos.x;
        float new_dy = player_pos.pos.y - enemy_pos.pos.y;
        float new_dist = sqrt(new_dx * new_dx + new_dy * new_dy);

        if (dist == 0 || new_dist < dist) {
          dx = new_dx;
          dy = new_dy;
          dist = new_dist;
        }
      }

      if (dist > 0) {
        enemy_vel.vel.x = dx / dist;
        enemy_vel.vel.y = dy / dist;
      }
    }
  }
};

class SCollide : public System<SCollide> {
public:
  SCollide() {
    queries[0] = Query(CPosition::get_id(), CSize::get_id());
    queries[1] = Query(CPosition::get_id(), CSize::get_id());
    query_count = 2;
  }

  void update(ECS &ecs) {
    auto entities_a = get_query(0)->get_entities();
    auto entities_b = get_query(1)->get_entities();
    Iterator it_a = {.next = 0};
    Iterator it_b = {.next = 0};
    Entity e_a, e_b;
    while (entities_a->next(it_a, e_a)) {
      CPosition &pos_a = ecs.get_component<CPosition>(e_a);
      CSize &size_a = ecs.get_component<CSize>(e_a);
      while (entities_b->next(it_b, e_b)) {
        // Skip self-collision
        if (e_a == e_b)
          continue;
        CPosition &pos_b = ecs.get_component<CPosition>(e_b);
        CSize &size_b = ecs.get_component<CSize>(e_b);

        if (pos_a.pos.x < pos_b.pos.x + size_b.scale.x &&
            pos_a.pos.x + size_a.scale.x > pos_b.pos.x &&
            pos_a.pos.y < pos_b.pos.y + size_b.scale.y &&
            pos_a.pos.y + size_a.scale.y > pos_b.pos.y) {
          // Collision detected
          printf("Collision detected between entity %d and entity %d\n", e_a,
                 e_b);
        }
      }
    }
  }
};

class SRender : public System<SRender> {
public:
  SRender() {
    queries[0] = Query(CPosition::get_id(), CSize::get_id(), CMesh::get_id());
    query_count = 1;
  }

  void update(ECS &ecs) {
    auto renderer = &ecs.get_resource<RRenderer>()->m_renderer;
    auto scene = ecs.get_resource<RScene>();
    auto hierarchy = scene->m_hierarchy;
    auto camera = scene->m_camera;
    int height;
    int width;

    auto entities = get_query(0)->get_entities();
    Iterator it = {.next = 0};
    Entity e;
    while (entities->next(it, e)) {
      auto pos = ecs.get_component<CPosition>(e);
      auto size = ecs.get_component<CSize>(e);
      auto mesh = ecs.get_component<CMesh>(e);
      hierarchy.m_nodes[mesh.m_node.get_value()].translation = pos.pos;
      hierarchy.m_nodes[mesh.m_node.get_value()].rotation = glm::vec3(0);
      hierarchy.m_nodes[mesh.m_node.get_value()].scale = size.scale;
    }

    // Draw
    renderer->clear();
    renderer->begin_pass(scene->m_scene, camera, width, height);
    renderer->draw_hierarchy(scene->m_scene, hierarchy);
    renderer->end_pass();

    gui::render();

    glfwSwapBuffers(scene->m_window);
  }
};

class SMoveCamera : public System<SMoveCamera> {
public:
  void update(ECS &ecs) {
    auto input = ecs.get_resource<RInput>()->m_input;
    auto scene = ecs.get_resource<RScene>();
    auto camera = &scene->m_camera;
    auto window = scene->m_window;

    if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
      bool mouse_locked =
          glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
      glfwSetInputMode(window, GLFW_CURSOR,
                       mouse_locked ? GLFW_CURSOR_NORMAL
                                    : GLFW_CURSOR_DISABLED);
    }

    glm::vec3 direction(0.0f);

    if (input.is_key_pressed(GLFW_KEY_W))
      direction.z += 1.0f;
    if (input.is_key_pressed(GLFW_KEY_S))
      direction.z -= 1.0f;
    if (input.is_key_pressed(GLFW_KEY_A))
      direction.x -= 1.0f;
    if (input.is_key_pressed(GLFW_KEY_D))
      direction.x += 1.0f;

    if (glm::length(direction) > 0) {
      if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
        camera->move(glm::normalize(direction), 0.016f);
      } else {
        auto forward = camera->m_orientation * glm::vec3(0, 0, -1);
        auto right = camera->m_orientation * glm::vec3(1, 0, 0);
        forward.y = 0;
        right.y = 0;

        forward = glm::normalize(forward);
        right = glm::normalize(right);
        auto movement = (right * direction.x + forward * direction.z) *
                        camera->m_speed * 0.016f;
        scene->m_scene.m_nodes[1].translation += movement;

        glm::quat target_rotation =
            glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));

        scene->m_scene.m_nodes[1].rotation = glm::slerp(
            scene->m_scene.m_nodes[1].rotation, target_rotation, 0.016f * 8.0f);

        // camera movement

        // glm::quat camera_target_rotation =
        // glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));
        // state.camera.m_orientation = glm::slerp(state.camera.m_orientation,
        // camera_target_rotation, state.delta_time); state.camera.m_pos =
        // state.scene.m_nodes[1].translation + state.camera.m_orientation *
        // glm::vec3(0,0,10);
        glm::vec3 camera_offset = glm::vec3(-5, 5, -5);
        glm::vec3 camera_target_position =
            scene->m_scene.m_nodes[1].translation + camera_offset;
        camera->m_pos =
            glm::mix(camera->m_pos, camera_target_position, 0.016f * 5);
        camera->m_orientation =
            glm::quatLookAt(glm::normalize(-camera_offset), glm::vec3(0, 1, 0));
      }
    }
    // mouse input

    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
      glm::vec2 mouse_delta = input.get_mouse_position_delta();
      camera->rotate(-mouse_delta.x * 0.001f, -mouse_delta.y * 0.001f);
    }
    input.update();
  }
};

class SLocalMove : public System<SLocalMove> {
public:
  SLocalMove() {
    queries[0] = Query(CVelocity::get_id(), CLocal::get_id());
    query_count = 1;
  }

  void update(ECS &ecs) {
    auto input = ecs.get_resource<RInput>()->m_input;
    auto scene = ecs.get_resource<RScene>();
    auto camera = &scene->m_camera;
    auto window = scene->m_window;
    auto local_player = get_query(0)->get_entities()->first();
    auto local_vel = ecs.get_component<CVelocity>(*local_player);

    if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
      bool mouse_locked =
          glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
      glfwSetInputMode(window, GLFW_CURSOR,
                       mouse_locked ? GLFW_CURSOR_NORMAL
                                    : GLFW_CURSOR_DISABLED);
    }

    glm::vec3 direction(0.0f);

    if (input.is_key_pressed(GLFW_KEY_W))
      direction.z += 1.0f;
    if (input.is_key_pressed(GLFW_KEY_S))
      direction.z -= 1.0f;
    if (input.is_key_pressed(GLFW_KEY_A))
      direction.x -= 1.0f;
    if (input.is_key_pressed(GLFW_KEY_D))
      direction.x += 1.0f;

    if (glm::length(direction) > 0) {
      if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
        camera->move(glm::normalize(direction), 0.016f);
      } else {
        auto forward = camera->m_orientation * glm::vec3(0, 0, -1);
        auto right = camera->m_orientation * glm::vec3(1, 0, 0);
        forward.y = 0;
        right.y = 0;

        forward = glm::normalize(forward);
        right = glm::normalize(right);

        auto movement = (right * direction.x + forward * direction.z) *
                        camera->m_speed * 0.016f;
        local_vel.x = movement.x;
        local_vel.y = movement.y;

        // glm::quat target_rotation =
        //     glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));
        //
        // scene->m_scene.m_nodes[1].rotation = glm::slerp(
        //     scene->m_scene.m_nodes[1].rotation, target_rotation, 0.016f
        //     * 8.0f);
      }
    }
  }
};

template <class... Args>
void fatal(std::format_string<Args...> fmt, Args &&...args) {
  ERROR(fmt, std::forward<Args>(args)...);
  exit(1);
}

static void error_callback(int error, const char *description) {
  ERROR("GLFW error with code {}: {}", error, description);
}

static void framebuffer_size_callback(GLFWwindow *window, int width,
                                      int height) {
  (void)window;
  glViewport(0, 0, width, height);
}

static void gen_world(State &state, engine::NodeHandle &root) {
  Map map(100, 15, 5, 10, 1337);

  engine::NodeHandle map_node = state.hierarchy.add_node(
      {
          .name = "Map",
          .rotation = glm::quat(1, 0, 0, 0),
          .scale = glm::vec3(5, 1, 5),
      },
      root);

  engine::MeshHandle cube_mesh = state.scene.mesh_by_name("Cube");

  for (size_t i = 0; i < map.grid.size(); ++i) {
    for (size_t j = 0; j < map.grid[i].size(); ++j) {
      if (map.grid[i][j] != -1) {
        state.hierarchy.add_node(
            {
                .kind = engine::Node::Kind::mesh,
                .name = std::format("cube_{}_{}", i, j),
                .rotation = glm::quat(1, 0, 0, 0),
                .translation = glm::vec3(map.grid.size() * -0.5 + (f32)i, -2.5,
                                         map.grid[i].size() * -0.5 + (f32)j),
                .scale = glm::vec3(1),
                .mesh_index = cube_mesh.get_value(),
            },
            map_node);
      }
    }
  }
}

int main(void) {
  if (!glfwInit()) {
    fatal("Failed to initiliaze glfw");
  }
  INFO("Initialized GLFW");
  glfwSetErrorCallback(error_callback);

  f32 content_x_scale;
  f32 content_y_scale;
  glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_x_scale,
                             &content_y_scale);
  INFO("Monitor content scale is ({}, {})", content_x_scale, content_y_scale);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 8);
#ifndef NDEBUG
  glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#else
  glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_TRUE);
#endif

  f32 content_scale = std::max(content_x_scale, content_y_scale);
  auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale,
                                 "Skeletal Animation", nullptr, nullptr);
  if (!window) {
    fatal("Failed to create glfw window");
  }

  glfwMakeContextCurrent(window);

  engine::Input input(window);
  State state = {};
  state.camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);
  state.mouse_locked = true;
  state.sensitivity = 0.001f;

  state.renderer.init((engine::Renderer::LoadProc)glfwGetProcAddress);
  {
    auto data = engine::loader::load_asset_file("scene_data.bin");
    state.scene.init(data);
    state.renderer.make_resources_for_scene(data);
  }

  engine::NodeHandle root_node = state.hierarchy.add_root_node({
      .name = "Game",
      .rotation = glm::quat(1, 0, 0, 0),
      .scale = glm::vec3(1),
  });

  gen_world(state, root_node);

  auto player_prefab = state.scene.prefab_by_name("Player");
  engine::NodeHandle player = state.hierarchy.instantiate_prefab(
      state.scene, player_prefab, engine::NodeHandle(0));

  auto enemy_prefab = state.scene.prefab_by_name("Enemy");
  engine::NodeHandle enemy = state.hierarchy.instantiate_prefab(
      state.scene, enemy_prefab, engine::NodeHandle(0));

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  int width;
  int height;
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  state.player.position = glm::vec3(0.0f);
  state.player.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  // Should probably always be 1
  state.player.scale = glm::vec3(1.0f);
  state.player.speed = 10;

  // Init enemy
  state.enemy.position = glm::vec3(20, 0, 20);
  state.enemy.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  state.enemy.scale = glm::vec3(1.0f);
  state.enemy.speed = 3;

  // Create ECS
  ECS ecs = ECS();

  // Register resources
  ecs.register_resource(new RInput(window));
  ecs.register_resource(new RRenderer(renderer));
  ecs.register_resource(new RScene(scene, camera, window));

  // Register componets
  ecs.register_component<CPlayer>();
  ecs.register_component<CPosition>();
  ecs.register_component<CVelocity>();
  ecs.register_component<CEnemy>();
  ecs.register_component<CSize>();
  ecs.register_component<CName>();

  // Register systems
  auto move_system = ecs.register_system<SMove>();
  auto enemy_ai_system = ecs.register_system<SEnemyMove>();
  // auto collide_system = ecs.register_system<SCollide>();
  auto render_system = ecs.register_system<SRender>();
  auto move_camera_system = ecs.register_system<SMoveCamera>();

  // Create local player
  Entity player = ecs.create_entity();
  ecs.add_component<CPlayer>(player, CPlayer());
  ecs.add_component<CPosition>(player, CPosition{.x = 10, .y = 10});
  ecs.add_component<CVelocity>(player, CVelocity{.x = 0, .y = 0});
  ecs.add_component<CSize>(player, CSize{.width = .2, .height = .2});
  ecs.add_component<CName>(player, CName{.name = "Player"});
  ecs.add_component<CMesh>(player, CMesh(helmet_mesh));

  // Create enemy
  // Entity enemy = ecs.create_entity();
  // ecs.add_component<CEnemy>(enemy, CEnemy());
  // ecs.add_component<CPosition>(enemy, CPosition{.x = 0, .y = 0});
  // ecs.add_component<CVelocity>(enemy, CVelocity{.x = 0, .y = 0});
  // ecs.add_component<CSize>(enemy, CSize{.width = .2, .height = .2});
  // ecs.add_component<CName>(enemy, CName{.name = "Enemy"});

  // Create Map
  Entity map = ecs.create_entity();
  ecs.add_component<CPosition>(map, CPosition{.x = 0, .y = 0});
  ecs.add_component(map, CName{.name = "sponza"});
  ecs.add_component(map, CSize{.width = 1, .height = 1});
  ecs.add_component<CMesh>(map, CMesh(sponza_mesh));

  while (!glfwWindowShouldClose(window)) {
    // Update
    glfwPollEvents();
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) {
      continue;
    }
    state.fb_width = width;
    state.fb_height = height;

    state.prev_delta_times[state.fps_counter_index] = state.delta_time;
    state.fps_counter_index =
        (state.fps_counter_index + 1) % state.prev_delta_times.size();
    // Update
    move_system->update(ecs);
    enemy_ai_system->update(ecs);
    // collide_system->update(ecs);
    move_camera_system->update(ecs);
    render_system->update(ecs);
  }
  forward = glm::normalize(forward);
  right = glm::normalize(right);
  auto movement = (right * direction.x + forward * direction.z) *
                  state.player.speed * state.delta_time;
  state.player.position += movement;

  glm::quat target_rotation =
      glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));

  state.player.rotation = glm::slerp(state.player.rotation, target_rotation,
                                     state.delta_time * 8.0f);

  // camera movement
  glm::vec3 camera_offset = glm::vec3(-5, 5, -5);
  glm::vec3 camera_target_position = state.player.position + camera_offset;
  state.camera.m_pos = glm::mix(state.camera.m_pos, camera_target_position,
                                state.delta_time * 5);
  state.camera.m_orientation =
      glm::quatLookAt(glm::normalize(-camera_offset), glm::vec3(0, 1, 0));

  // ememy movement
  {
    glm::vec3 enemy_move = state.player.position - state.enemy.position;
    enemy_move.y = 0;
    if (enemy_move.x != 0 || enemy_move.z != 0)
      enemy_move = glm::normalize(enemy_move);

    glm::quat target_rotation = glm::quatLookAt(enemy_move, glm::vec3(0, 1, 0));
    state.enemy.position += enemy_move * state.enemy.speed * state.delta_time;
    state.enemy.rotation = glm::slerp(state.enemy.rotation, target_rotation,
                                      state.delta_time * 8.0f);
  }

  glfwPollEvents();
  gui::build(state);

  // Not the formal way to set a node transform. Just temporary
  state.hierarchy.m_nodes[player.get_value()].translation =
      state.player.position;
  state.hierarchy.m_nodes[player.get_value()].rotation = state.player.rotation;
  state.hierarchy.m_nodes[player.get_value()].scale = state.player.scale;

  state.hierarchy.m_nodes[enemy.get_value()].translation = state.enemy.position;
  state.hierarchy.m_nodes[enemy.get_value()].rotation = state.enemy.rotation;
  state.hierarchy.m_nodes[enemy.get_value()].scale = state.enemy.scale;
}
