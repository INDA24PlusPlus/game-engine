#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <print>

#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/core.h"
#include "engine/utils/logging.h"
#include "glm/fwd.hpp"
#include "glm/gtc/quaternion.hpp"
#include "gui.h"
#include "state.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
    state.scene.load_asset_file("scene_data.bin");
    state.scene.compute_global_node_transforms();
    state.renderer.make_resources_for_scene(state.scene);

    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gui::init(window, content_scale);

    engine::MeshHandle helmet_mesh = state.scene.mesh_from_name("SciFiHelmet");
    engine::MeshHandle sponza_mesh = state.scene.mesh_from_name("Sponza");

    // Someway to the store the player transform for now.
    // just temporaryily.
    auto player_position = glm::vec3(0.0f);
    auto player_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // Should probably always be 1
    auto player_scale = glm::vec3(1.0f);

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

        if (glm::length(direction) > 0) {
            if (input.is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
                state.camera.move(glm::normalize(direction), state.delta_time);
            } else {
                auto forward = state.camera.m_orientation * glm::vec3(0, 0, -1);
                auto right = state.camera.m_orientation * glm::vec3(1, 0, 0);
                forward.y = 0;
                right.y = 0;

                forward = glm::normalize(forward);
                right = glm::normalize(right);
                auto movement = (right * direction.x + forward * direction.z) *
                                state.camera.m_speed * state.delta_time;
                player_position += movement;

                glm::quat target_rotation =
                    glm::quatLookAt(glm::normalize(movement), glm::vec3(0, 1, 0));

                player_rotation =
                    glm::slerp(player_rotation, target_rotation, state.delta_time * 8.0f);

                // camera movement

                // glm::quat camera_target_rotation = glm::quatLookAt(glm::normalize(movement),
                // glm::vec3(0, 1, 0)); state.camera.m_orientation =
                // glm::slerp(state.camera.m_orientation, camera_target_rotation, state.delta_time);
                // state.camera.m_pos = state.scene.m_nodes[1].translation +
                // state.camera.m_orientation * glm::vec3(0,0,10);
                glm::vec3 camera_offset = glm::vec3(-5, 5, -5);
                glm::vec3 camera_target_position = player_position + camera_offset;
                state.camera.m_pos =
                    glm::mix(state.camera.m_pos, camera_target_position, state.delta_time * 5);
                state.camera.m_orientation =
                    glm::quatLookAt(glm::normalize(-camera_offset), glm::vec3(0, 1, 0));
            }
        }

        // mouse input
        if (state.mouse_locked) {
            glm::vec2 mouse_delta = input.get_mouse_position_delta();
            state.camera.rotate(-mouse_delta.x * state.sensitivity,
                                -mouse_delta.y * state.sensitivity);
        }

        glfwPollEvents();
        state.scene.compute_global_node_transforms();
        gui::build(state);

        glm::mat4 player_transform;
        {
            auto T = glm::translate(glm::mat4(1.0f), player_position);
            auto R = glm::mat4_cast(player_rotation);
            auto S = glm::scale(glm::mat4(1.0f), player_scale);
            player_transform = T * R * S;
        }

        // Draw
        state.renderer.clear();
        state.renderer.begin_pass(state.camera, width, height);
        state.renderer.draw_mesh(state.scene, helmet_mesh, player_transform);
        state.renderer.draw_mesh(state.scene, sponza_mesh);
        state.renderer.end_pass();

        gui::render();

        glfwSwapBuffers(window);

        input.update();
    }
}
