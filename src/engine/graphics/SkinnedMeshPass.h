#ifndef _SKINNED_MESH_PASS
#define _SKINNED_MESH_PASS

#include "../assets.h"

#include "vk.h"
#include "constants.h"
#include "Buffer.h"

namespace renderer {

struct Context;

struct UBOMatrices {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

struct SkinnedMeshPass {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet ubo_descriptors[max_frames_in_flight];

    Buffer ubo_buffers[max_frames_in_flight];
    UBOMatrices* ubo_matrices[max_frames_in_flight];

    Buffer bone_ubo_buffers[max_frames_in_flight];
    glm::mat4* gpu_bone_transformations[max_frames_in_flight];

    Buffer vertex_buffer;

    void init(Context* context, Buffer vertex_buffer, u32 num_bones);
    void deinit(Context* context);
    void render(VkCommandBuffer cmd, Slice<engine::asset::SkinnedMesh> meshes, u32 current_image_index, u32 display_bone);
    void create_descriptor_sets(Context* context, u32 num_bones);
    void create_descriptor_set_layout(Context* context);
    void create_uniform_buffers(Context* context, u32 num_bones);
    void create_pipeline(Context* context);
    void update_uniform_buffers(Context* context, const glm::mat4& view_matrix);
};

}

#endif