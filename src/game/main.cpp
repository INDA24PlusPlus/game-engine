#include <algorithm>
#include <print>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "engine/Scene.h"
#include "engine/utils/logging.h"
#include "engine/core.h"
#include "engine/Renderer.h"

#include "state.h"
#include "gui.h"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

template<class... Args>
void fatal(std::format_string<Args...> fmt, Args&&... args) {
    ERROR(fmt, std::forward<Args>(args)...);
    exit(1);
}

static void error_callback(int error, const char* description) {
   ERROR("GLFW error with code {}: {}", error, description);
}


static void process_input(State& state, GLFWwindow* window, engine::Camera& camera, float delta_time) {
    static bool esc_pressed = false;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!esc_pressed) {
            if (state.mouse_locked) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            state.mouse_locked = !state.mouse_locked;
        }
        esc_pressed = true;
    } else {
        esc_pressed = false;
    }

    glm::vec3 direction(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction.x += 1.0f;

    if (glm::length(direction) > 0) {
        camera.move(glm::normalize(direction), delta_time);
    }
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    auto state = (State*)glfwGetWindowUserPointer(window);
    if (!state->mouse_locked) return;

    static float lastX = xpos, lastY = ypos;
    float xoffset = lastX - xpos;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    state->camera.rotate(xoffset * state->sensitivity, yoffset * state->sensitivity);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
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
    auto window = glfwCreateWindow(1920 / content_scale, 1080 / content_scale, "Skeletal Animation", nullptr, nullptr);
    if (!window) {
       fatal("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    State state;
    state.camera.init(glm::vec3(0.0f, 0.0f, 3.0f), 10.0f);
    state.mouse_locked = true;
    state.sensitivity = 0.001f;

    engine::Renderer renderer((engine::Renderer::LoadProc)glfwGetProcAddress);
    state.scene.load_asset_file("scene_data.bin");
    renderer.make_resources_for_scene(state.scene);


    glfwSetWindowUserPointer(window, &state);
    glfwSetCursorPosCallback(window, mouse_callback);
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

        glfwPollEvents();
        process_input(state, window, state.camera, state.delta_time);
        state.scene.compute_global_node_transforms();
        gui::build(state);

        // Draw
        renderer.clear();
        renderer.begin_pass(state.camera, width, height);
        renderer.draw_mesh(state.scene, helmet_mesh);
        renderer.draw_mesh(state.scene, sponza_mesh);
        renderer.end_pass();

        gui::render();

        glfwSwapBuffers(window);
    }
}
