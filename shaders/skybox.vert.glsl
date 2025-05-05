#version 460 core

layout (location = 0) in vec3 a_pos;
layout (location = 0) out vec3 tex_coords;


// Reuse UBO from the other vertex shader for now.
layout (std140, binding = 0) uniform UBOMatrices {
    mat4 model;
    mat4 view;
    mat4 projection;
};

void main() {
    tex_coords = a_pos;
    vec4 pos = projection * view * vec4(a_pos, 1.0);
    gl_Position = pos.xyww;
}