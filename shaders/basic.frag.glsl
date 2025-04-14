#version 460 core

layout(location = 0)
out vec4 o_color;

in vec3 in_normal;
in vec3 in_frag_pos;
in vec2 in_uv;

layout (binding = 0)
uniform sampler2D s_texture;

layout (location = 0)
uniform uint u_material_flags;

layout (location = 1)
uniform vec4 u_base_color_factor;

const uint MATERIAL_FLAG_BASE_COLOR_TEXUTE = 1 << 0;

vec3 light_dir = vec3(0.2, 0.7, 0.5);

vec3 get_base_color() {
    if ((u_material_flags & MATERIAL_FLAG_BASE_COLOR_TEXUTE) != 0) {
        return texture(s_texture, in_uv).rgb * u_base_color_factor.rgb;
    }

    return u_base_color_factor.rgb;
}

void main() {
    vec3 normal = normalize(in_normal);
    float diffuse = max(dot(normal, normalize(light_dir)), 0.0); 
    vec3 result = vec3(diffuse) + vec3(0.1);
    vec3 base_color = get_base_color();
    o_color = vec4(base_color * result, 1.0);
}
