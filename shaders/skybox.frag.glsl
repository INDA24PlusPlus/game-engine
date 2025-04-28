#version 460

layout(location = 0)
out vec4 o_color;

layout (location = 0)
in vec3 tex_coords;

layout (binding = 0) uniform samplerCube skybox;

void main() {
    vec3 color = texture(skybox, normalize(tex_coords)).rgb;
    o_color = vec4(color / (color + vec3(1.0)), 1.0);
}