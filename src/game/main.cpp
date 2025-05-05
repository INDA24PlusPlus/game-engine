#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <print>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/AssetLoader.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/core.h"
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


template <class... Args>
void fatal(std::format_string<Args...> fmt, Args &&...args) {
    ERROR(fmt, std::forward<Args>(args)...);
    exit(1);
}

static void error_callback(int error, const char *description) {
    ERROR("GLFW error with code {}: {}", error, description);
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
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
                        .translation = glm::vec3(map.grid.size() * -0.5 + (f32)i, -2.5, map.grid[i].size() * -0.5 + (f32)j),
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
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_x_scale, &content_y_scale);
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
    auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale, "Skeletal Animation",
                                   nullptr, nullptr);
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
    engine::NodeHandle player =
        state.hierarchy.instantiate_prefab(state.scene, player_prefab, engine::NodeHandle(0));

    auto enemy_prefab = state.scene.prefab_by_name("Enemy");
    engine::NodeHandle enemy =
        state.hierarchy.instantiate_prefab(state.scene, enemy_prefab, engine::NodeHandle(0));
    engine::NodeHandle enemy2 =
        state.hierarchy.instantiate_prefab(state.scene, enemy_prefab, engine::NodeHandle(0));

    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gui::init(window, content_scale);
    
    // init camera orientation
    state.camera.m_orientation =
        glm::quatLookAt(glm::normalize(glm::vec3(-3, -1, -3)), glm::vec3(0, 1, 0));

    // init enemy2
    state.enemy2.position = -state.enemy2.position;
    state.enemy2.speed = 4;
    state.enemy2.hover_time = 1;

    while (!glfwWindowShouldClose(window)) {
        state.prev_time = state.curr_time;
        state.curr_time = glfwGetTime();
        state.delta_time = state.curr_time - state.prev_time;

        // Update
        glfwPollEvents();
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) {
            continue;
        }
        state.fb_width = width;
        state.fb_height = height;

        state.prev_delta_times[state.fps_counter_index] = state.delta_time;
        state.fps_counter_index = (state.fps_counter_index + 1) % state.prev_delta_times.size();

        // keyboard input
        if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
            state.mouse_locked = !state.mouse_locked;
            glfwSetInputMode(window, GLFW_CURSOR,
                             state.mouse_locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        glm::vec3 direction(0.0f);

        if (input.is_key_pressed(GLFW_KEY_W)) direction.z += 1.0f;
        if (input.is_key_pressed(GLFW_KEY_S)) direction.z -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_A)) direction.x -= 1.0f;
        if (input.is_key_pressed(GLFW_KEY_D)) direction.x += 1.0f;

        if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
            if (glm::length(direction) > 0) {
                state.camera.move(glm::normalize(direction), state.delta_time);
            }
        } else {
            auto forward = state.camera.m_orientation * glm::vec3(0, 0, -1);
            auto right = state.camera.m_orientation * glm::vec3(1, 0, 0);
            forward.y = 0;
            right.y = 0;

            forward = glm::normalize(forward);
            right = glm::normalize(right);
            auto movement = (right * direction.x + forward * direction.z) * state.player.speed *
                            state.delta_time;
            state.player.position += movement;

            if (glm::length(direction) > 0) {
                glm::quat target_rotation =
                    glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));
                    
                state.player.rotation =
                    glm::slerp(state.player.rotation, target_rotation, state.delta_time * 8.0f);
            }

            // camera movement
            float camera_distance = 10;
            glm::vec3 camera_target_position = state.player.position 
                - state.camera.m_orientation * glm::vec3(0, 0, -1) * camera_distance;
            state.camera.m_pos =
                glm::mix(state.camera.m_pos, camera_target_position, state.delta_time * 5);
            
        }

        // ememy update
        state.enemy.update(state);
        state.enemy2.update(state);

        // mouse input
        if (state.mouse_locked) {
            glm::vec2 mouse_delta = input.get_mouse_position_delta();
            state.camera.rotate(-mouse_delta.x * state.sensitivity,
                                -mouse_delta.y * state.sensitivity);
        }

        glfwPollEvents();
        gui::build(state);

        // Not the formal way to set a node transform. Just temporary
        state.hierarchy.m_nodes[player.get_value()].translation = state.player.position;
        state.hierarchy.m_nodes[player.get_value()].rotation = state.player.rotation;
        state.hierarchy.m_nodes[player.get_value()].scale = state.player.scale;

        state.hierarchy.m_nodes[enemy.get_value()].translation = state.enemy.position;
        state.hierarchy.m_nodes[enemy.get_value()].rotation = state.enemy.rotation;
        state.hierarchy.m_nodes[player.get_value()].scale = state.enemy.scale;

        state.hierarchy.m_nodes[enemy2.get_value()].translation = state.enemy2.position;
        state.hierarchy.m_nodes[enemy2.get_value()].rotation = state.enemy2.rotation;
        state.hierarchy.m_nodes[enemy2.get_value()].scale = state.enemy2.scale;

        // Draw
        state.renderer.clear();

        state.renderer.begin_pass(state.scene, state.camera, width, height);
        state.renderer.draw_hierarchy(state.scene, state.hierarchy);
        state.renderer.end_pass();

        state.renderer.begin_rect_pass();
        state.renderer.draw_rect({ .x = 100, .y = 100, .width = 1000, .height = 1000}, glm::vec3(1));
        state.renderer.end_rect_pass(state.fb_width, state.fb_height);

        gui::render();

        glfwSwapBuffers(window);

        input.update();
    }
}
