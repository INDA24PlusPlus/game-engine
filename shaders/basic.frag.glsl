#version 460 core

layout(location = 0)
out vec4 o_color;

in vec3 in_normal;
in vec3 in_frag_pos;
in vec2 in_uv;

layout (binding = 0)
uniform sampler2D s_texture;

vec3 light_dir = vec3(0.2, 0.7, 0.5);

void main() {
    vec3 normal = normalize(in_normal);
    float diffuse = max(dot(normal, normalize(light_dir)), 0.0); 
    vec3 result = vec3(diffuse) + vec3(0.1);
    vec3 base_color = texture(s_texture, in_uv).rgb;
    o_color = vec4(base_color * result, 1.0);
}
