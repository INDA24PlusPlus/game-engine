#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <tuple>

namespace engine {
class Input {
public:
    Input(GLFWwindow* window);
    Input() = delete;

    // Call this AFTER every frame
    void update();

    bool is_key_pressed(int key);
    bool is_key_released(int key);
    bool is_key_just_pressed(int key);
    bool is_key_just_released(int key);

    bool is_mouse_button_pressed(int button);
    bool is_mouse_button_released(int button);
    bool is_mouse_button_just_pressed(int button);
    bool is_mouse_button_just_released(int button);

    glm::vec2 get_mouse_position();
    glm::vec2 get_mouse_position_delta();

private:
    GLFWwindow* window;
    
    bool prevKeyStates[GLFW_KEY_LAST + 1];
    bool prevMouseButtonStates[GLFW_MOUSE_BUTTON_LAST + 1];
    glm::vec2 prevMousePosition = {0.0, 0.0};
};
}