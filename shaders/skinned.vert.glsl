#version 460 core

layout(location = 0)
in vec3 a_pos;

layout(location = 1)
in vec2 a_uv;

layout(location = 2)
in uvec4 a_joints;

layout(location = 3)
in vec4 a_weights;

layout(location = 0)
uniform mat4 u_mvp;

layout (std140, binding = 0) uniform BoneTransforms {
    mat4 bone_transforms[23];
};

out vec2 uv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(0.0);
    mat4 bone_transform = bone_transforms[a_joints[0]] * a_weights[0];
    bone_transform += bone_transforms[a_joints[1]] * a_weights[1];
    bone_transform += bone_transforms[a_joints[2]] * a_weights[2];
    bone_transform += bone_transforms[a_joints[3]] * a_weights[3];

    vec4 bone_pos = bone_transform * vec4(a_pos, 1.0);
    gl_Position = u_mvp * bone_pos;

    uv = a_uv;
}
