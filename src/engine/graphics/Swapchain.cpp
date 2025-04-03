#include "Swapchain.h"

#include <print>

#include "context.h"
#include "vulkan/vulkan_core.h"

namespace renderer {

void Swapchain::init(Context* context, VkExtent2D extent, VkSwapchainKHR old_handle, VkSurfaceFormatKHR surface_format, VkPresentModeKHR present_mode, u32 image_count) {
    std::println("Trying to create swapchain with extent {}x{}", extent.width, extent.height);
    m_surface_format = surface_format;
    m_present_mode = present_mode;

    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->pdev, context->surface, &caps));

    if (caps.currentExtent.width != 0xFFFFFFFF) {
        m_extent = caps.currentExtent;
    } else {
        m_extent.width = clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height = clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    std::println("Actual swapchain extent {}x{}", m_extent.width, m_extent.height);

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .flags = {},
        .surface = context->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0, // NOTE: Both of these are only used when sharing is concurrent.
        .pQueueFamilyIndices = nullptr,
        .preTransform = caps.currentTransform, // TODO: See if can make sure this is identity transform.
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_handle,
    };

    VK_CHECK(vkCreateSwapchainKHR(context->device, &create_info, nullptr, &m_handle));
    if (old_handle != nullptr) {
        vkDestroySwapchainKHR(context->device, old_handle, nullptr);
    }

    VkImage image_handles[3];

    uint32_t count;
    vkGetSwapchainImagesKHR(context->device, m_handle, &count, nullptr);
    assert(count < 3);
    vkGetSwapchainImagesKHR(context->device, m_handle, &count, image_handles);

    for (uint32_t i = 0; i < count; ++i) {
        m_images[i].init(context, surface_format.format, image_handles[i]); 
    }
    m_frames_in_flight = count;
    std::println("Swapchain has {} frames in flight", m_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &m_next_image_acquired));

    VK_CHECK(vkAcquireNextImageKHR(context->device, m_handle, 0xFFFFFFFFFFFFFFFF, m_next_image_acquired, nullptr, &m_image_index));
    swap(&m_images[m_image_index].m_acquired, &m_next_image_acquired);
    std::println("Swapchain successfully created");
}


void Swapchain::deinit_resources(Context* context) {
    for (size_t i = 0; i < m_frames_in_flight; ++i) {
        m_images[i].deinit(context);
    }
    vkDestroySemaphore(context->device, m_next_image_acquired, nullptr);
}

void Swapchain::deinit(Context* context) {
    deinit_resources(context);
    vkDestroySwapchainKHR(context->device, m_handle, nullptr);
}

void Swapchain::wait_for_fences(Context* context) {
    VkFence fences[3] = {
        m_images[0].m_frame_fence,
        m_images[1].m_frame_fence,
        m_images[2].m_frame_fence,
    };

    VK_CHECK(vkWaitForFences(context->device, m_frames_in_flight, fences, VK_TRUE, 0xFFFFFFFFFFFFFFFF));
}

void SwapImage::init(Context* context, VkFormat format, VkImage image) {
    m_image = image;

    VkComponentMapping components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };

    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = {},
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = components,
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VK_CHECK(vkCreateImageView(context->device, &view_create_info, nullptr, &m_view));

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &m_acquired));
    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &m_render_finished));

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VK_CHECK(vkCreateFence(context->device, &fence_create_info, nullptr, &m_frame_fence));
}

void SwapImage::deinit(Context* context) {
    VkResult result = vkWaitForFences(context->device, 1, &m_frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF);
    if (result != VK_SUCCESS) {
        std::println("Error {} when waitning for fence for swap image", vk_result_to_string(result));
    }

    vkDestroyImageView(context->device, m_view, nullptr);
    vkDestroySemaphore(context->device, m_acquired, nullptr);
    vkDestroySemaphore(context->device, m_render_finished, nullptr);
    vkDestroyFence(context->device, m_frame_fence, nullptr);
}
}