#version 460 core

layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec4 a_color;

layout (location = 0) out vec4 color;

layout (std140, binding = 0) uniform UBOMatrices {
    mat4 model;
    mat4 view;
    mat4 projection;
};

void main() {
    gl_Position = projection * vec4(a_pos, 0.0, 1.0);
    color = a_color;
}