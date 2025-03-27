#version 450

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_pos;
layout(location = 1) out vec3 v_normal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} PerMesh;

void main() {
    gl_Position = PerMesh.projection * PerMesh.view * PerMesh.model * vec4(a_pos, 1.0);
    v_pos = vec3(PerMesh.model * vec4(a_pos, 1.0));
    v_normal = a_normal;
}
