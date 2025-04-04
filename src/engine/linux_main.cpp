#include <print>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "platform.h"

#include "game/game.h"

static void error_callback(int error_code, const char* description) {
    std::println("GLFW error ({}): {}", error_code, description);
}

int main(void) {
    if (!glfwInit()) {
        platform::fatal("Failed to initialize glfw");
    }
    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(600, 400, "Vulkan renderer", nullptr,nullptr);
    if (!window) {
        platform::fatal("Failed to create glfw window");
    }

    game::State game_state;

    f32 curr_time = 0.0f;
    f32 prev_time = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        game::update(game_state, )
    }
}
