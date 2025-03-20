#version 450

layout(location = 0) in vec3 a_pos;

layout(location = 0) out vec3 v_color;

layout (push_constant) uniform per_mesh {
    mat4 model;
    mat4 view;
    mat4 projection;
} PerMesh;

void main() {
    gl_Position = PerMesh.projection * PerMesh.view * PerMesh.model * vec4(a_pos, 1.0);
    v_color = vec3(1.0f, 0.0f, 0.0f);
}
