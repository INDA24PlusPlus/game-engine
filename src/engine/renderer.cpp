#include "renderer.h"

#include "arena.h"
#include "graphics/context.h"

#include "graphics/vk.h"

// unity build for now..
#include "graphics/context.cpp"
#include "graphics/Swapchain.cpp"
#include "graphics/SkinnedMeshPass.cpp"
#include "graphics/Buffer.cpp"
#include "assets.cpp"

namespace renderer {

[[nodiscard]] Context* initialize(const char* app_name, u32 fb_width, u32 fb_height, platform::WindowInfo window_info) {
    size_t allocation_size = MB(1);
    u8* memory = (u8*)platform::allocate(allocation_size);
    Arena scratch = arena_init(memory, allocation_size);
    auto context = arena_create<Context>(&scratch);
    context->scratch = scratch;
    context->init(app_name, fb_width, fb_height, window_info);
    return context;
}

void destroy_context(Context* context) {
    context->deinit();
}

void resize_swapchain(Context* context, u32 width, u32 height) {
    vkDeviceWaitIdle(context->device);

    auto swapchain = &context->swapchain;
    swapchain->deinit_resources(context);
    swapchain->init(context, { .width = width, .height = height}, swapchain->m_handle, swapchain->m_surface_format, swapchain->m_present_mode, swapchain->m_frames_in_flight);

    context->state.deinit_depth_resources(context);
    context->state.init_depth_resources(context);

    context->state.deinit_command_pool_and_buffers(context);
    context->state.init_command_pool_and_buffers(context);
}

PresentState present(Context* context, const glm::mat4& view_matrix, f32 curr_time) {
    auto swapchain = &context->swapchain;

    // Record commands for the current frame.
    context->state.record_commands(context);

    // Wait for the current frame to finish rendering.
    auto current = swapchain->m_images[swapchain->m_image_index];
    VK_CHECK(vkWaitForFences(context->device, 1, &current.m_frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF));
    VK_CHECK(vkResetFences(context->device, 1, &current.m_frame_fence));


    traverse_node_hierarchy(context, context->state.node_hierarchy[0], glm::mat4(1.0f), curr_time);
    context->state.m_skinned_mesh_pass.update_uniform_buffers(context, view_matrix);

    // Submit command buffer.
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current.m_acquired, // <----- wait until we have acquired the image from the swapchain.
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &context->state.m_cmd_bufs[swapchain->m_image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &current.m_render_finished, // <------- render_finished is signaled once the queue has been executed.
    };

    VK_CHECK(vkQueueSubmit(context->graphics_queue.handle, 1, &submit_info, current.m_frame_fence)); // <--- submit and signal frame fence once we are finished.

    // Present the frame
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current.m_render_finished, // <----- we wait until we have rendered into the image we will be presenting.
        .swapchainCount = 1,
        .pSwapchains = &swapchain->m_handle,
        .pImageIndices = &swapchain->m_image_index,
        .pResults = nullptr
    };

    VkResult result = vkQueuePresentKHR(context->graphics_queue.handle, &present_info);
    switch (result) {
        case VK_SUBOPTIMAL_KHR: return PresentState::suboptimal;
        case VK_ERROR_OUT_OF_DATE_KHR: return PresentState::suboptimal;
        default: VK_CHECK(result);
    };


    // Acquire next frame
    u32 next_frame_index;
    result = vkAcquireNextImageKHR(context->device, swapchain->m_handle, 0xFFFFFFFFFFFFFFFF, swapchain->m_next_image_acquired, nullptr, &next_frame_index);
    swap(&swapchain->m_images[next_frame_index].m_acquired, &swapchain->m_next_image_acquired);

    // reset the command pool for the next frame.
    VK_CHECK(vkWaitForFences(context->device, 1, &swapchain->m_images[next_frame_index].m_frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF));

    VK_CHECK(vkResetCommandPool(context->device, context->state.m_pools[next_frame_index], 0));
    swapchain->m_image_index = next_frame_index;

    switch (result) {
        case VK_SUBOPTIMAL_KHR: return PresentState::suboptimal;
        default: VK_CHECK(result);
    };

    return PresentState::optimal;
}

void increment_display_bone(Context* context) {
    context->state.display_bone = (context->state.display_bone + 1) % context->state.skinned_meshes[0].num_bones;
}

}