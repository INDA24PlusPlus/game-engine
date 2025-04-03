#ifndef _SWAPCHAIN_H
#define _SWAPCHAIN_H

#include "vk.h"
#include "constants.h"
#include "../core.h"

namespace renderer {

struct Context;

struct SwapImage {
    VkImage m_image;
    VkImageView m_view;
    VkSemaphore m_acquired;
    VkSemaphore m_render_finished;
    VkFence m_frame_fence;

    void init(Context* context, VkFormat format, VkImage image);
    void deinit(Context* context);
};

struct Swapchain {
    VkSwapchainKHR m_handle;
    VkExtent2D m_extent;
    VkSurfaceFormatKHR m_surface_format;
    VkPresentModeKHR m_present_mode;

    // We will never have more than 3 images in the swapchain.
    SwapImage m_images[max_frames_in_flight];
    u32 m_frames_in_flight;
    u32 m_image_index;
    VkSemaphore m_next_image_acquired;

    void init(Context* context, VkExtent2D extent, VkSwapchainKHR old_handle, VkSurfaceFormatKHR surface_format, VkPresentModeKHR present_mode, u32 image_count);
    void deinit(Context* context);
    void deinit_resources(Context* context);
    void wait_for_fences(Context* context);
};

}

#endif