#version 450

layout(location = 0) in vec3 a_pos;
layout(location = 1) in uvec4 a_bone_indices;
layout(location = 2) in vec4 a_bone_weights;

layout(location = 0) out float weight;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} PerMesh;

layout(set = 0, binding = 1) uniform BoneTransformation {
    mat4 bone_transforms[19]; // TODO: Use specilzation constant maybe?
};

layout (push_constant) uniform constatns {
	uint display_bone;
} PushConstants;

void main() {
    mat4 bone_transform = bone_transforms[a_bone_indices[0]] * a_bone_weights[0];
    bone_transform += bone_transforms[a_bone_indices[1]] * a_bone_weights[1];
    bone_transform += bone_transforms[a_bone_indices[2]] * a_bone_weights[2];
    bone_transform += bone_transforms[a_bone_indices[3]] * a_bone_weights[3];

    vec4 bone_pos = bone_transform * vec4(a_pos, 1.0);
    gl_Position = PerMesh.projection * PerMesh.view * PerMesh.model * bone_pos;

    weight = 0.0;
    for (int i = 0; i < 4; ++i) {
        if (a_bone_indices[i] == PushConstants.display_bone) {
            weight = a_bone_weights[i];
        }
    }
}
