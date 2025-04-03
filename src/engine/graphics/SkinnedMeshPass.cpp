#include "SkinnedMeshPass.h"

#include <array>
#include "context.h"
#include "engine/assets.h"
#include "vulkan/vulkan_core.h"

namespace renderer {
void SkinnedMeshPass::init(Context* context, Buffer vertex_buffer, u32 num_bones) {
    // Descriptors
    this->create_uniform_buffers(context, num_bones);
    this->create_descriptor_set_layout(context);
    this->create_descriptor_sets(context, num_bones);

    this->create_pipeline(context);
    this->vertex_buffer = vertex_buffer;
}

void SkinnedMeshPass::deinit(Context* context) {
    vkDestroyPipeline(context->device, pipeline, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context->device, descriptor_set_layout, nullptr);

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        auto mat_buf = ubo_buffers[i];
        auto bone_buf = bone_ubo_buffers[i];
        mat_buf.deinit(context->vma_allocator);
        bone_buf.deinit(context->vma_allocator);
    }
}

void SkinnedMeshPass::render(VkCommandBuffer cmd, Slice<engine::asset::SkinnedMesh> meshes, u32 current_image_index, u32 display_bone) {
    (void)current_image_index;
    if (meshes.len == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &ubo_descriptors[current_image_index], 0, nullptr);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer.handle, &offset);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(u32), &display_bone);

    for (size_t i = 0; i < meshes.len; ++i) {
        vkCmdDrawIndexed(cmd, meshes[i].num_indices, 1, meshes[i].base_index, meshes[i].base_vertex, 0);
    }
}

void SkinnedMeshPass::create_descriptor_sets(Context* context, u32 num_bones) {
    VkDescriptorSetLayout layouts[max_frames_in_flight];

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        layouts[i] = descriptor_set_layout;
    }

    context->allocate_descriptor_sets({ .ptr = layouts, .len = max_frames_in_flight }, { .ptr = ubo_descriptors, .len = max_frames_in_flight });

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        VkDescriptorBufferInfo matrices_buffer_info = {
            .buffer = ubo_buffers[i].handle,
            .offset = 0,
            .range = sizeof(UBOMatrices),
        };

        VkDescriptorBufferInfo bone_buffer_info = {
            .buffer = bone_ubo_buffers[i].handle,
            .offset = 0,
            .range = num_bones * sizeof(glm::mat4),
        };

        std::array<VkWriteDescriptorSet, 2> writes = {
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = ubo_descriptors[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &matrices_buffer_info,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = ubo_descriptors[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &bone_buffer_info,
            },
        };

        vkUpdateDescriptorSets(context->device, writes.size(), writes.data(), 0, nullptr);
    }
}

void SkinnedMeshPass::create_descriptor_set_layout(Context* context) {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
        VkDescriptorSetLayoutBinding {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,.pImmutableSamplers = nullptr
        },
        VkDescriptorSetLayoutBinding {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        }
    };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = bindings.size(),
      .pBindings = bindings.data(),
  };

  VK_CHECK(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &descriptor_set_layout));
}

void SkinnedMeshPass::create_uniform_buffers(Context* context, u32 num_bones) {
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        ubo_buffers[i] = Buffer::init(context->vma_allocator,
                                       sizeof(UBOMatrices),
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        ubo_matrices[i] = (UBOMatrices*)ubo_buffers[i].info.pMappedData;

        bone_ubo_buffers[i] = Buffer::init(context->vma_allocator,
                                       sizeof(glm::mat4) * num_bones,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        gpu_bone_transformations[i] = (glm::mat4*)bone_ubo_buffers[i].info.pMappedData;
    }
}

void SkinnedMeshPass::create_pipeline(Context* context) {
    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(u32),
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant,
    };
    VK_CHECK(vkCreatePipelineLayout(context->device, &layout_info, nullptr, &pipeline_layout));

    std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions = {{
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(engine::asset::SkinnedVertex, pos)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UINT, .offset = offsetof(engine::asset::SkinnedVertex, bone_indices)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(engine::asset::SkinnedVertex, bone_weights)},
    }};
    Slice<VkVertexInputAttributeDescription> attribs = { .ptr = attribute_descriptions.data(), .len = attribute_descriptions.size() };

    VkShaderModule vertex_shader = context->load_shader_module("shaders/skinning_vert.spv");
    VkShaderModule fragment_shader = context->load_shader_module("shaders/skinning_frag.spv");
    pipeline = context->build_pipeline(pipeline_layout, attribs, sizeof(engine::asset::SkinnedVertex), vertex_shader, fragment_shader);
}

void SkinnedMeshPass::update_uniform_buffers(Context* context, const glm::mat4& view_matrix) {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), (f32)context->swapchain.m_extent.width / (f32)context->swapchain.m_extent.height, 0.1f, 10000.f);
    proj[1][1] *= -1;

    UBOMatrices mat_data = {
        .model = glm::translate(context->state.mesh_transforms[0], glm::vec3(0, 0, 0)),
        .view = view_matrix,
        .projection = proj,
    };
    memcpy(ubo_matrices[context->swapchain.m_image_index], &mat_data, sizeof(UBOMatrices));

    for (size_t i = 0; i < context->state.bone_data.len; i++) {
        gpu_bone_transformations[context->swapchain.m_image_index][i] = context->state.bone_data[i].final;
    }
}

}