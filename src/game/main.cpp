#include "engine/Camera.h"
#include <glad/glad.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#include <sys/socket.h>

#include "components.h"
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
#include "engine/ecs/system.hpp"
#include "engine/network/client.hpp"
#include "engine/network/network.hpp"
#include "engine/scene/Node.h"
#include "engine/scene/Scene.h"
#include "engine/utils/logging.h"
#include "glm/detail/qualifier.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_common.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "settings.h"
#include "state.h"
#include "world_gen/map.h"
#include "time.h"
#include "util.h"

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

static void gen_world(engine::NodeHierarchy &hierarchy, engine::Scene &scene,
                      engine::NodeHandle &root) {
  Map map(100, 15, 5, 10, 1337);

  engine::NodeHandle map_node = hierarchy.add_node(
      {
          .name = "Map",
          .rotation = glm::quat(1, 0, 0, 0),
          .scale = glm::vec3(5, 1, 5),
      },
      root);

  engine::MeshHandle cube_mesh = scene.mesh_by_name("Cube");

  for (size_t i = 0; i < map.grid.size(); ++i) {
    for (size_t j = 0; j < map.grid[i].size(); ++j) {
      if (map.grid[i][j] != -1) {
        hierarchy.add_node(
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

int main(int argc, char **argv) {
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

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  // Create ECS
  ECS ecs = ECS();

  // Register resources
  INFO("Create resources");
  engine::Scene scene;
  engine::Input input(window);
  engine::NodeHierarchy hierarchy;

  engine::NodeHandle root_node = hierarchy.add_root_node({
      .name = "Game",
      .rotation = glm::quat(1, 0, 0, 0),
      .scale = glm::vec3(1),
  });

  engine::Camera camera;
  camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);

  engine::Renderer renderer;
  renderer.init((engine::Renderer::LoadProc)glfwGetProcAddress);
  {
    auto data = engine::loader::load_asset_file("scene_data.bin");
    scene.init(data);
    renderer.make_resources_for_scene(data);
  }

  int width;
  int height;
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  gen_world(hierarchy, scene, root_node);

  // Register componets
  INFO("Create componets");
  ecs.register_component<CPlayer>();
  ecs.register_component<CSpeed>();
  ecs.register_component<CHealth>();
  ecs.register_component<CEnemyGhost>();
  ecs.register_component<CTranslation>();
  ecs.register_component<CVelocity>();
  ecs.register_component<CMesh>();
  ecs.register_component<COnline>();

  // Register systems
  INFO("Create systems");
  auto move_system = ecs.register_system<SMove>();
  auto render_system = ecs.register_system<SRender>();
  // auto ui_render_system = ecs.register_system<SUIRender>();
  auto player_controller_system = ecs.register_system<SPlayerController>();
  auto camera_system = ecs.register_system<SCamera>();
  auto delta_time_system = ecs.register_system<SDeltaTime>();
  auto input_system = ecs.register_system<SInput>();
  auto get_message_system = ecs.register_system<SGetMessage>();
  auto send_online_position_system = ecs.register_system<SSendOnlinePosition>();

  // Create local player
  INFO("Create player");
  Entity player = ecs.create_entity();
  ecs.add_component<CPlayer>(player, CPlayer());
  ecs.add_component<CLocal>(player, CLocal());
  ecs.add_component<CSpeed>(player, CSpeed());
  ecs.add_component<CHealth>(player, CHealth());
  ecs.add_component<CTranslation>(
      player, CTranslation{.pos = glm::vec3(0.0f),
                           .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           .scale = glm::vec3(1.0f)});
  ecs.add_component<CVelocity>(player, CVelocity{.vel = glm::vec3(0.0f)});

  auto player_prefab = scene.prefab_by_name("Player");
  engine::NodeHandle player_node =
      hierarchy.instantiate_prefab(scene, player_prefab, engine::NodeHandle(0));

  ecs.add_component<CMesh>(player, CMesh(player_node));


  // Register resources
  ecs.register_resource(new RInput(window));
  ecs.register_resource(new RRenderer(renderer));
  ecs.register_resource(new RScene(scene, camera, window, hierarchy, player));
  ecs.register_resource(new RDeltaTime(glfwGetTime()));
  ecs.register_resource(new RMultiplayerClient(argv[1], argv[2], ecs));

  INFO("Begin game loop");

  while (!glfwWindowShouldClose(window)) {
    // Update
    delta_time_system->update(ecs); // updates delta_time
    player_controller_system->update(
        ecs); // player controller for local player(movement and stuff)
    camera_system->update(ecs);      // camera control
    move_system->update(ecs);        // moves entities with velocity

    input_system->update(ecs); // stores this frames keys as previous frames
                               // keys (do this last, before draw)

    get_message_system->update(ecs);
    send_online_position_system->update(ecs);

    // Draw
    render_system->update(ecs);    // handle rendering
    // ui_render_system->update(ecs); // handle rendering of UI

    glfwSwapBuffers(window);
  }
}
