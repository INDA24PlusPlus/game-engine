#include "Game.h"

#include "utils/logging.h"

static void error_callback(int error, const char *description) {
    ERROR("GLFW error with code {}: {}", error, description);
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

game::GameHandle game::init(int width, int height, const std::string& title)
{
    if (!glfwInit()) {
        FATAL("Failed to initiliaze glfw");
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
                                title.c_str(), nullptr, nullptr);
    if (!window) {
        FATAL("Failed to create glfw window");
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    return game::GameHandle {
        .window = window,
    };
}

bool game::should_run(GameHandle& handle)
{
    return !glfwWindowShouldClose(handle.window);
}

void game::end_frame(GameHandle& handle)
{
    glfwSwapBuffers(handle.window);
    glfwPollEvents();
}
