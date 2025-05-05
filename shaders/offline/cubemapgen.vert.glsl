#version 460 core
layout (location = 0) in vec3 a_pos;

layout (location = 0) out vec3 local_pos;


// Reuse from base vertex shader.
layout (std140, binding = 0) uniform UBOMatrices {
    mat4 model;
    mat4 view;
    mat4 projection;
};

void main() {
    local_pos = a_pos;  
    gl_Position =  projection * view * vec4(local_pos, 1.0);
}