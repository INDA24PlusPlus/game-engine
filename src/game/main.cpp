#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <print>

#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/core.h"
#include "engine/utils/logging.h"

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
    State state;
    state.camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);
    state.mouse_locked = true;
    state.sensitivity = 0.001f;

    engine::Renderer renderer((engine::Renderer::LoadProc)glfwGetProcAddress);
    state.scene.load_asset_file("scene_data.bin");
    state.scene.compute_global_node_transforms();
    renderer.make_resources_for_scene(state.scene);

    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gui::init(window, content_scale);

    auto helmet_mesh = state.scene.mesh_from_name("SciFiHelmet");
    auto sponza_mesh = state.scene.mesh_from_name("Sponza");

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
            state.camera.move(glm::normalize(direction), state.delta_time);
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

        // Draw
        renderer.clear();
        renderer.begin_pass(state.camera, width, height);
        renderer.draw_mesh(state.scene, helmet_mesh);
        renderer.draw_mesh(state.scene, helmet_mesh, glm::translate(glm::mat4(1.0f), glm::vec3(0, 2, 0)));
        renderer.draw_mesh(state.scene, sponza_mesh);
        renderer.end_pass();

        gui::render();

        glfwSwapBuffers(window);

        input.update();
    }
}
