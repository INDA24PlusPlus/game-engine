#pragma once

#include <GLFW/glfw3.h>

struct Settings {
    float sensitivity = 1.5;
    
    int key_forward = GLFW_KEY_W;
    int key_backward = GLFW_KEY_S;
    int key_left = GLFW_KEY_A;
    int key_right = GLFW_KEY_D;
    
    int key_attack = GLFW_KEY_SPACE;
};