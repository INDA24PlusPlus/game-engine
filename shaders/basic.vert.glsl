#version 460 core

layout(location = 0)
in vec4 a_tangent;

layout(location = 1)
in vec3 a_pos;

layout(location = 2)
in vec3 a_normal;

layout(location = 3)
in vec2 a_uv;

layout (std140, binding = 0) uniform UBOMatrices {
    mat4 model;
    mat4 view;
    mat4 projection;
};

layout (location = 0) out vec3 in_normal;
layout (location = 1) out vec3 in_frag_pos;
layout (location = 2) out vec2 in_uv;
layout (location = 3) out mat3 in_TBN;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = projection * view * model * vec4(a_pos, 1.0);
    in_uv = a_uv;
    in_frag_pos = vec3(model * vec4(a_pos, 1.0));

    mat3 normal_matrix = transpose(inverse(mat3(model)));
    in_normal = normal_matrix * a_normal;

    vec3 T = normalize(vec3(model * vec4(a_tangent.xyz, 0.0)));
    vec3 N = normalize(in_normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(a_normal, T) * a_tangent.w;
    in_TBN = mat3(T, B, N);
}
