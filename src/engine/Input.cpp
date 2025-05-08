#include "Input.h"
#include <print>

engine::Input::Input(GLFWwindow* window) : window{window}, prevKeyStates{false}, prevMousePosition(get_mouse_position()) {}

void engine::Input::update() {
    for(int key = GLFW_KEY_SPACE; key < GLFW_KEY_LAST + 1; key++){
        prevKeyStates[key] = glfwGetKey(window, key);
    }

    for(int button = GLFW_MOUSE_BUTTON_1; button < GLFW_MOUSE_BUTTON_LAST + 1; button++){
        prevMouseButtonStates[button] = glfwGetMouseButton(window, button);
    }

    prevMousePosition = get_mouse_position();
}

bool engine::Input::is_key_pressed(int key) {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool engine::Input::is_key_released(int key) {
    return glfwGetKey(window, key) == GLFW_RELEASE;
}

bool engine::Input::is_key_just_pressed(int key) {
    return glfwGetKey(window, key) == GLFW_PRESS && prevKeyStates[key] == GLFW_RELEASE;
}

bool engine::Input::is_key_just_released(int key) {
    return glfwGetKey(window, key) == GLFW_RELEASE && prevKeyStates[key] == GLFW_PRESS;
}

bool engine::Input::is_mouse_button_pressed(int button) {
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool engine::Input::is_mouse_button_released(int button) {
    return glfwGetMouseButton(window, button) == GLFW_RELEASE;
}

bool engine::Input::is_mouse_button_just_pressed(int button) {
    return glfwGetMouseButton(window, button) == GLFW_PRESS && prevMouseButtonStates[button] == GLFW_RELEASE;
}

bool engine::Input::is_mouse_button_just_released(int button) {
    return glfwGetMouseButton(window, button) == GLFW_RELEASE && prevMouseButtonStates[button] == GLFW_PRESS;
}

glm::vec2 engine::Input::get_mouse_position(){
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return {x, y};
}

glm::vec2 engine::Input::get_mouse_position_delta(){
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    return {
        (float)x - prevMousePosition.x, 
        (float)y - prevMousePosition.y
    };
}