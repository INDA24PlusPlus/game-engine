#version 450

layout(location = 0) in float weight;

layout(location = 0) out vec4 f_color;

void main() {
	f_color = vec4(mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), weight), 1.0);
}
