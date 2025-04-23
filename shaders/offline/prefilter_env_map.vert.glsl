#version 460 core
layout (location = 0) in vec3 a_pos;

layout (location = 0) out vec3 local_pos;

layout (location = 5)
uniform mat4 projection;

layout (location = 6)
uniform mat4 view;

void main() {
    local_pos = a_pos;  
    gl_Position =  projection * view * vec4(local_pos, 1.0);
}