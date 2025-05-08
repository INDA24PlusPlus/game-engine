#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "core.h"

namespace game {
    struct GameHandle {
        GLFWwindow* window;
    };

    GameHandle init(int width, int height, const std::string& title);

    bool should_run(GameHandle& handle);
    void end_frame(GameHandle& handle);
}