#version 460 core

layout(location = 0)
in vec3 a_pos;

layout(location = 1)
in vec3 a_normal;

layout(location = 2)
in vec2 a_uv;

layout(location = 0)
uniform mat4 model;

layout(location = 1)
uniform mat4 view;

layout(location = 2)
uniform mat4 projection;

out vec3 in_normal;
out vec3 in_frag_pos;
out vec2 in_uv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = projection * view * model * vec4(a_pos, 1.0);
    in_uv = a_uv;
    in_normal = a_normal;
    in_frag_pos = vec3(model * vec4(a_pos, 1.0));
}
