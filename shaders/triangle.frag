#version 450

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec3 v_normal;

layout(location = 0) out vec4 f_color;

const vec3 light_pos = vec3(0);
const vec3 ambient = vec3(0.1);

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(light_pos - v_pos);
    vec3 diffuse = vec3(max(dot(normal, light_dir), 0.0));
    f_color = vec4(diffuse + ambient, 1.0);
}
