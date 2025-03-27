#include "renderer.h"

// Stuff we know till need device memory allocations
// - Vertex buffer
// - Depth buffer

// To keep in mind
// - When we want to do shadow mapping add read bit to the memory barrier for the depth attachment.
// - Might have to change load/store op for the color attachment when switching to a HDR color buffer

// FIXME: Don't use print here. I do this for now to make sure
// this file has proper log messages, but we don't actually have a logging system right now.

#include "core.h"
#include "platform.h"
#include "arena.h"
#include "assets.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "volk.h"

#include <print>
#include <format>
#include <optional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <VmaUsage.h>
#include <tracy/Tracy.hpp>

namespace renderer {

constexpr u32 max_frames_in_flight = 3;

struct QueueAllocation {
    u32 graphics_family; // NOTE: For now we require the graphics queue to support present operations.
};

struct Buffer {
    VkBuffer handle;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct PhysicalDevice {
    VkPhysicalDevice pdev;
    VkPhysicalDeviceProperties props;
    QueueAllocation queues; 
};

struct Queue {
    VkQueue handle;
    u32 family;
};

struct SwapImage {
    VkImage image;
    VkImageView view;
    VkSemaphore acquired;
    VkSemaphore render_finished;
    VkFence frame_fence;
};

struct Swapchain {
    VkSwapchainKHR handle;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;

    // We will never have more than 3 images in the swapchain.
    SwapImage images[max_frames_in_flight];
    u32 frames_in_flight;
    u32 image_index;
    VkSemaphore next_image_acquired;
};

struct UBOMatrices {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

struct Pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
};

struct RenderState {
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    Pipeline skinned_pipeline;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_sets[max_frames_in_flight];

    Buffer static_vertex_buffer;
    Buffer skinned_vertex_buffer;
    Buffer index_buffer;

    Buffer ubo_buffers[max_frames_in_flight];
    UBOMatrices* ubo_matrices[max_frames_in_flight];

    VkCommandPool pools[max_frames_in_flight];
    VkCommandBuffer cmd_bufs[max_frames_in_flight];

    VkImage depth_image;
    VmaAllocation depth_image_allocation;
    VkImageView depth_image_view;
    VkFormat depth_format;

    Slice<glm::mat4> mesh_transforms;
    Slice<engine::asset::StaticMesh> static_meshes;
    Slice<engine::asset::SkinnedMesh> skinned_meshes;
    Slice<engine::asset::Node> node_hierarchy;
    Slice<engine::asset::BoneData> bone_data;

    u32 display_bone;
};

struct Context {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice pdev;
    VkDevice device;
    VkPhysicalDeviceProperties pdev_props;
    VmaAllocator vma_allocator;

    Queue graphics_queue;
    Swapchain swapchain;

    Arena scratch;

    // TEMP.
    RenderState state;
};

// There is no logic to this. Just go ham :) It is also here since I don't want to bring in vulkan.h to the header file
// and it will stay that way until I decide the make a abstraction for the graphics api.
static VkSurfaceKHR create_surface(VkInstance instance, platform::WindowInfo window_info);
static PhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, Arena* scratch);
static VkDevice init_physical_device(const PhysicalDevice* pdev);
static const char* vk_result_to_string(VkResult result);
static bool is_present_mode_supported(Context* context, VkPresentModeKHR present_mode);
static VkSurfaceFormatKHR find_surface_format(Context* context, VkSurfaceFormatKHR preferred);
static void swapchain_init(Context* context, VkExtent2D extent, VkSwapchainKHR old_handle, VkSurfaceFormatKHR surface_format, VkPresentModeKHR present_mode, u32 image_count);
static void swapchain_deinit_resources(Context* context);
static void swapchain_deinit(Context* context);
static void destroy_temp_state(Context* context);
static void wait_for_swap_image_fences(Context* context);
static bool check_validation_support(Context* context);
static void render_mesh(Context* context);
static void destroy_command_pools(Context* context);
static void create_command_pool_and_buffers(Context* context);
static void destroy_depth_resources(Context* context);
static void create_depth_resources(Context* context);
static Slice<u8> read_entire_file_or_crash(Arena* arena_region, const char* path);
static void create_vma_allocator(Context* context);
static Buffer create_buffer(Context* context,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VmaAllocationCreateFlags vma_flags);
static void update_uniform_buffers(Context* context, f32 curr_time, const glm::mat4& view_matrix);

#ifdef _WIN32
constexpr std::array<const char*, 2> instance_extensions = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface"
};
#endif

constexpr std::array<const char*, 1> required_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr std::array<const char*, 1> validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr bool enable_validation_layers = true;
constexpr uint32_t num_validation_layers = enable_validation_layers ? validation_layers.size() : 0;

#define VK_CHECK(f)                                                                              \
    do {                                                                                         \
        VkResult res = (f);                                                                      \
        if (res != VK_SUCCESS) {                                                                 \
            platform::fatal(std::format("Fatal Vulkan error: {}", vk_result_to_string(res)));    \
        }                                                                                        \
    } while (0)


[[nodiscard]] Context* initialize(const char* app_name, u32 fb_width, u32 fb_height, platform::WindowInfo window_info) {
    size_t allocation_size = MB(1);
    u8* memory = (u8*)platform::allocate(allocation_size);
    Arena scratch = arena_init(memory, allocation_size);
    auto context = arena_create<Context>(&scratch);
    context->scratch = scratch;

    std::println("Initializing Vulkan context");
    VK_CHECK(volkInitialize());
    std::println("Loaded Vulkan procedures");

    if (enable_validation_layers && !check_validation_support(context)) {
        platform::fatal("Validation layers are enabled but your system does not support them, download the VulkanSDK!");
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = app_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 0),
        .pEngineName = app_name, // TODO: Change to actual engine name.
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 0),
        .apiVersion = VK_MAKE_API_VERSION(1, 3, 0, 0),
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .pApplicationInfo = &app_info,
        .enabledLayerCount = num_validation_layers,
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = instance_extensions.size(),
        .ppEnabledExtensionNames = instance_extensions.data(),
    };

    context->instance = nullptr;
    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &context->instance));
    volkLoadInstance(context->instance);
    std::println("Created Vulkan instance");

    context->surface = create_surface(context->instance, window_info);
    std::println("Created Vulkan surface");

    auto physical_device = pick_physical_device(context->instance, context->surface, &context->scratch);
    context->pdev = physical_device.pdev;
    context->pdev_props = physical_device.props;
    std::println("Selected device {}", physical_device.props.deviceName);

    context->device = init_physical_device(&physical_device);
    volkLoadDevice(context->device);
    std::println("Created device interface");

    vkGetDeviceQueue(context->device, physical_device.queues.graphics_family, 0, &context->graphics_queue.handle);
    context->graphics_queue.family = physical_device.queues.graphics_family;

    create_vma_allocator(context);

    // NOTE: FIFO is always supported. When chaning this use 'is_present_mode_supported' before specifying anything else.
    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkSurfaceFormatKHR preferred_swapchain_surface_format = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    VkSurfaceFormatKHR swapchain_surface_format = find_surface_format(context, preferred_swapchain_surface_format);

    // FIXME: hardcoded to 2 frames in flight.
    u32 frames_in_flight = 2;
    swapchain_init(context, { .width = fb_width, .height = fb_height }, nullptr, swapchain_surface_format, swapchain_present_mode, frames_in_flight);

    // Quick memory leak check.
    assert(context->scratch.pos == sizeof(Context));

    return context;
}

void destroy_context(Context* context) {
    std::println("Destroying context...");

    // Wait until swapchain is finished.
    if (context->swapchain.handle != nullptr) {
        wait_for_swap_image_fences(context);
    }
    vkDeviceWaitIdle(context->device);

    destroy_temp_state(context);
    if (context->swapchain.handle != nullptr) {
        swapchain_deinit(context);
    }

    vmaDestroyAllocator(context->vma_allocator); 

    vkDestroyDevice(context->device, nullptr);
    vkDestroySurfaceKHR(context->instance, context->surface, nullptr);
    vkDestroyInstance(context->instance, nullptr);
    std::println("Destroyed context");
}

void increment_display_bone(Context* context) {
    context->state.display_bone = (context->state.display_bone + 1) % context->state.skinned_meshes[0].num_bones;
}

void resize_swapchain(Context* context, u32 width, u32 height) {
    vkDeviceWaitIdle(context->device);

    auto swapchain = &context->swapchain;
    swapchain_deinit_resources(context);
    swapchain_init(context, { .width = width, .height = height}, swapchain->handle, swapchain->surface_format, swapchain->present_mode, swapchain->frames_in_flight);

    destroy_depth_resources(context);
    create_depth_resources(context);

    destroy_command_pools(context);
    create_command_pool_and_buffers(context);
}

PresentState present(Context* context, f32 curr_time, const glm::mat4& view_matrix) {
    ZoneScopedN("Present");
    auto swapchain = &context->swapchain;

    // Record commands for the current frame.
    render_mesh(context);

    // Wait for the current frame to finish rendering.
    auto current = swapchain->images[swapchain->image_index];
    {
        ZoneScopedN("Wait for previous frame");
        VK_CHECK(vkWaitForFences(context->device, 1, &current.frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF));
        VK_CHECK(vkResetFences(context->device, 1, &current.frame_fence));
    }

    update_uniform_buffers(context, curr_time, view_matrix);

    // Submit command buffer.
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current.acquired, // <----- wait until we have acquired the image from the swapchain.
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &context->state.cmd_bufs[swapchain->image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &current.render_finished, // <------- render_finished is signaled once the queue has been executed.
    };
    {
        ZoneScopedN("Queue submission");
        VK_CHECK(vkQueueSubmit(context->graphics_queue.handle, 1, &submit_info, current.frame_fence)); // <--- submit and signal frame fence once we are finished.
    }

    // Present the frame
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current.render_finished, // <----- we wait until we have rendered into the image we will be presenting.
        .swapchainCount = 1,
        .pSwapchains = &swapchain->handle,
        .pImageIndices = &swapchain->image_index,
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
    result = vkAcquireNextImageKHR(context->device, context->swapchain.handle, 0xFFFFFFFFFFFFFFFF, context->swapchain.next_image_acquired, nullptr, &next_frame_index);
    swap(&swapchain->images[next_frame_index].acquired, &swapchain->next_image_acquired);

    // reset the command pool for the next frame.
    VK_CHECK(vkWaitForFences(context->device, 1, &swapchain->images[next_frame_index].frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF));

    VK_CHECK(vkResetCommandPool(context->device, context->state.pools[next_frame_index], 0));
    swapchain->image_index = next_frame_index;

    switch (result) {
        case VK_SUBOPTIMAL_KHR: return PresentState::suboptimal;
        default: VK_CHECK(result);
    };

    return PresentState::optimal;
}

static void create_vma_allocator(Context* context) {
    // Flags we probably want:
    // VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT - Get current VRAM usage?
    // VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT - Remove usage of internal mutexes (We will probably be single threaded).
    VmaVulkanFunctions vma_vulkan_functions = {
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory= vkAllocateMemory,
        .vkFreeMemory= vkFreeMemory,
        .vkMapMemory= vkMapMemory,
        .vkUnmapMemory= vkUnmapMemory,
        .vkFlushMappedMemoryRanges= vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges= vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory= vkBindBufferMemory,
        .vkBindImageMemory= vkBindImageMemory,
        .vkGetBufferMemoryRequirements= vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements= vkGetImageMemoryRequirements,
        .vkCreateBuffer= vkCreateBuffer,
        .vkDestroyBuffer= vkDestroyBuffer,
        .vkCreateImage= vkCreateImage,
        .vkDestroyImage= vkDestroyImage,
        .vkCmdCopyBuffer= vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        .vkBindImageMemory2KHR = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
        .vkGetMemoryWin32HandleKHR = vkGetMemoryWin32HandleKHR,
    };
    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = context->pdev,
        .device = context->device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = context->instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    VK_CHECK(vmaCreateAllocator(&allocator_create_info, &context->vma_allocator));
}

static bool check_extension_support(VkPhysicalDevice pdev, Arena* scratch) {
    auto tmp_region = arena_begin_region(scratch);
    defer { region_reset(&tmp_region); };

    uint32_t count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(pdev, nullptr, &count, nullptr));
    auto ext_props = arena_alloc<VkExtensionProperties>(scratch, count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(pdev, nullptr, &count, ext_props.ptr));

    uint32_t num_found = 0;
    for (auto ext : required_device_extensions) {
        for (size_t i = 0; i < ext_props.len; ++i) {
            auto ext_prop = ext_props[i];
            if (strcmp(ext_prop.extensionName, ext) == 0) {
                std::println("Found {} device extensions", ext);
                ++num_found;
                break;
            }
        }
    }

    return num_found == required_device_extensions.size();
}

static bool check_surface_support(VkPhysicalDevice pdev, VkSurfaceKHR surface) {
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdev, surface, &format_count, nullptr);

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdev, surface, &present_mode_count, nullptr);

    return format_count > 0 && present_mode_count > 0;
}

static bool check_feature_support(VkPhysicalDevice pdev) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pdev, &props);

    if (props.apiVersion < VK_API_VERSION_1_3) {
        std::println("Physical device {} does not support Vulkan 1.3", props.deviceName);
        return false;
    }

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = nullptr,
    };

    VkPhysicalDeviceVulkan12Features vulkan12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &dynamic_state_ext,
    };

    VkPhysicalDeviceVulkan13Features vulkan13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vulkan12_features,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan13_features,
    };

    vkGetPhysicalDeviceFeatures2(pdev, &features);

    if (!vulkan13_features.dynamicRendering) {
        std::println("Physical device {} does not support feature \"dynamic rendering\"", props.deviceName);
        return false;
    }
    if (!vulkan13_features.synchronization2) {
        std::println("Physical device {} does not support feature \"synchronization2\"", props.deviceName);
        return false;
    }
    if (!vulkan12_features.bufferDeviceAddress) {
        std::println("Physical device {} does not support feature \"buffer device address\"", props.deviceName);
        return false;
    }

    if (!dynamic_state_ext.extendedDynamicState) {
        std::println("Physical device {} does not support feature \"extended dynamic rendering\"", props.deviceName);
        return false;
    }

    return true;
}


static std::optional<QueueAllocation> allocate_queues(VkPhysicalDevice pdev, VkSurfaceKHR surface, Arena* scratch) {
    auto tmp = arena_begin_region(scratch);
    defer { region_reset(&tmp); };

    uint32_t family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, nullptr);
    auto families = arena_alloc<VkQueueFamilyProperties>(scratch, family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &family_count, families.ptr);

    std::optional<uint32_t> graphics_family = std::nullopt;
    for (size_t i = 0; i < families.len; ++i) {
        auto properties = &families[i];

        VkBool32 present_supported;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(pdev, i, surface, &present_supported));

        bool is_graphics_queue = (properties->queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        if (!graphics_family.has_value() && is_graphics_queue) {
            if (present_supported) {
                graphics_family = i;
            } else {
                std::println("Found graphics queue that does not support present commands!");
            }
        }
    }

    if (graphics_family.has_value()) {
        return QueueAllocation{
            .graphics_family = graphics_family.value(),
        };
    }
    std::println("Unable to find a suitable graphics queue");

    return {};
}

static std::optional<PhysicalDevice> check_suitable(VkPhysicalDevice pdev, VkSurfaceKHR surface, Arena* scratch) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pdev, &props);

    if (!check_extension_support(pdev, scratch)) {
        std::println("Physical device {} does not support required extensions.", props.deviceName);
    }

    if (!check_surface_support(pdev, surface)) {
        std::println("Physical device {} does not support appropriate surface and present modes.", props.deviceName);
    }
    if (!check_feature_support(pdev)) {
        std::println("Physical device {} does not support required Vulkan features.", props.deviceName);
    }

    auto queue_allocation = allocate_queues(pdev, surface, scratch);
    if (queue_allocation.has_value()) {
        std::println("Physical device {} meets all requirements", props.deviceName);
        return PhysicalDevice {
            .pdev = pdev,
            .props = props,
            .queues = queue_allocation.value(),
        };
    }

    return {};
}

static PhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, Arena* scratch) {
    auto tmp = arena_begin_region(scratch);
    defer { region_reset(&tmp); };

    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    auto devices = arena_alloc<VkPhysicalDevice>(tmp.arena, count);
    vkEnumeratePhysicalDevices(instance, &count, devices.ptr);

    for (size_t i = 0; i < devices.len; i++) {
        auto maybe_device = check_suitable(devices[i], surface, scratch);
        if (maybe_device.has_value()) {
            return maybe_device.value();
        }
    }

    platform::fatal("Failed to find a compatible video adapter");
}

static VkSurfaceKHR create_surface(VkInstance instance, platform::WindowInfo window_info) {
    VkSurfaceKHR surface;
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = (HINSTANCE)window_info.window_handle,
        .hwnd = (HWND)window_info.surface_handle,
    };
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface));
#endif
    return surface;
}

static VkDevice init_physical_device(const PhysicalDevice* pdev) {
    f32 priority = 1.0;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    uint32_t queue_count = 1;


    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state_ext = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = nullptr,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features vulkan12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &dynamic_state_ext,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vulkan13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vulkan12_features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan13_features,
        .features = {},
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = queue_count,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = required_device_extensions.size(),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };

    VkDevice device;
    VK_CHECK(vkCreateDevice(pdev->pdev, &create_info, nullptr, &device));
    return device;
}

static SwapImage init_swap_image(Context* context, VkFormat format, VkImage image) {
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

    VkImageView view;
    VK_CHECK(vkCreateImageView(context->device, &view_create_info, nullptr, &view));

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore acquired;
    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &acquired));

    VkSemaphore render_finished;
    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &render_finished));

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkFence frame_fence;
    VK_CHECK(vkCreateFence(context->device, &fence_create_info, nullptr, &frame_fence));

    return {
        .image = image,
        .view = view,
        .acquired = acquired,
        .render_finished = render_finished,
        .frame_fence = frame_fence,
    };
}

static void deinit_swap_image(Context* context, SwapImage* image) {
    VkResult result = vkWaitForFences(context->device, 1, &image->frame_fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF);
    if (result != VK_SUCCESS) {
        std::println("Error {} when waitning for fence for swap image", vk_result_to_string(result));
    }
    vkDestroyImageView(context->device, image->view, nullptr);
    vkDestroySemaphore(context->device, image->acquired, nullptr);
    vkDestroySemaphore(context->device, image->render_finished, nullptr);
    vkDestroyFence(context->device, image->frame_fence, nullptr);
}

// TODO: We want this function to be able to recover from swapcahin recreation failure.
static void swapchain_init(Context* context, VkExtent2D extent, VkSwapchainKHR old_handle, VkSurfaceFormatKHR surface_format, VkPresentModeKHR present_mode, u32 image_count) {
    std::println("Trying to create swapchain with extent {}x{}", extent.width, extent.height);
    context->swapchain.surface_format = surface_format;
    context->swapchain.present_mode = present_mode;

    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->pdev, context->surface, &caps));

    if (caps.currentExtent.width != 0xFFFFFFFF) {
        context->swapchain.extent = caps.currentExtent;
    } else {
        context->swapchain.extent.width = clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        context->swapchain.extent.height = clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    std::println("Actual swapchain extent {}x{}", context->swapchain.extent.width, context->swapchain.extent.height);

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .flags = {},
        .surface = context->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = context->swapchain.extent,
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

    VK_CHECK(vkCreateSwapchainKHR(context->device, &create_info, nullptr, &context->swapchain.handle));
    if (old_handle != nullptr) {
        vkDestroySwapchainKHR(context->device, old_handle, nullptr);
    }

    VkImage image_handles[3];

    uint32_t count;
    vkGetSwapchainImagesKHR(context->device, context->swapchain.handle, &count, nullptr);
    assert(count < 3);
    vkGetSwapchainImagesKHR(context->device, context->swapchain.handle, &count, image_handles);

    for (uint32_t i = 0; i < count; ++i) {
        context->swapchain.images[i] = init_swap_image(context, surface_format.format, image_handles[i]); 
    }
    context->swapchain.frames_in_flight = count;
    std::println("Swapchain has {} frames in flight", context->swapchain.frames_in_flight);

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK_CHECK(vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &context->swapchain.next_image_acquired));

    VK_CHECK(vkAcquireNextImageKHR(context->device, context->swapchain.handle, 0xFFFFFFFFFFFFFFFF, context->swapchain.next_image_acquired, nullptr, &context->swapchain.image_index));
    swap(&context->swapchain.images[context->swapchain.image_index].acquired, &context->swapchain.next_image_acquired);
    std::println("Swapchain successfully created");
}

static void swapchain_deinit_resources(Context* context) {
    std::println("Destroying swapchain resources");
    auto swapchain = &context->swapchain;
    for (u32 i = 0; i < swapchain->frames_in_flight; ++i) {
        deinit_swap_image(context, &swapchain->images[i]);
    }
    vkDestroySemaphore(context->device, swapchain->next_image_acquired, nullptr);
    std::println("Destroyed swapchain resources");
}

static void swapchain_deinit(Context* context) {
    std::println("Destroying swapchain");
    swapchain_deinit_resources(context);
    vkDestroySwapchainKHR(context->device, context->swapchain.handle, nullptr);
    std::println("Destroyed swapchain");
}

static bool is_present_mode_supported(Context* context, VkPresentModeKHR present_mode) {
    auto tmp_region = arena_begin_region(&context->scratch);
    defer { region_reset(&tmp_region); };

    u32 count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->pdev, context->surface, &count, nullptr));
    auto present_modes = arena_alloc<VkPresentModeKHR>(&context->scratch, count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(context->pdev, context->surface, &count, present_modes.ptr));

    for (size_t i = 0; i < present_modes.len; i++) {
        if (present_modes[i] == present_mode) {
            return true;
        }
    }

    return false;
}

static VkSurfaceFormatKHR find_surface_format(Context* context, VkSurfaceFormatKHR preferred) {
    auto tmp_region = arena_begin_region(&context->scratch);
    defer { region_reset(&tmp_region); };

    u32 count;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->pdev, context->surface, &count, nullptr));
    auto surface_formats = arena_alloc<VkSurfaceFormatKHR>(&context->scratch, count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->pdev, context->surface, &count, surface_formats.ptr));

    for (size_t i = 0; i < surface_formats.len; ++i) {
        auto surface_format = surface_formats[i];
        if (surface_format.format == preferred.format && surface_format.colorSpace == preferred.colorSpace) {
            std::println("Preferred surface format is supported.");
            return preferred;
        }
    }

    // Vulkan spec states that there is atleast one supported surface format.
    return surface_formats[0];
};

static void wait_for_swap_image_fences(Context* context) {
    VkFence fences[3] = {
        context->swapchain.images[0].frame_fence,
        context->swapchain.images[1].frame_fence,
        context->swapchain.images[2].frame_fence,
    };

    VK_CHECK(vkWaitForFences(context->device, context->swapchain.frames_in_flight, fences, VK_TRUE, 0xFFFFFFFFFFFFFFFF));
}


static bool check_validation_support(Context* context) {
    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    auto tmp = arena_begin_region(&context->scratch);
    defer { region_reset(&tmp); };

    auto layers = arena_alloc<VkLayerProperties>(&context->scratch, layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.ptr);
    for (size_t i = 0; i < validation_layers.size(); ++i) {
        bool layer_found = false;
        for (size_t j = 0; i < layers.len; ++j) {
            if (strcmp(validation_layers[i], layers[j].layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

const char* vk_result_to_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS: Command completed successfully";
        case VK_NOT_READY: return "VK_NOT_READY: A fence or query has not yet completed";
        case VK_TIMEOUT: return "VK_TIMEOUT: A wait operation has not completed in the specified time";
        case VK_EVENT_SET: return "VK_EVENT_SET: An event is signaled";
        case VK_EVENT_RESET: return "VK_EVENT_RESET: An event is unsignaled";
        case VK_INCOMPLETE: return "VK_INCOMPLETE: A return array was too small for the result";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY: Host memory allocation failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY: Device memory allocation failed";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED: Initialization of an object failed";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST: The logical or physical device has been lost";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED: Mapping of a memory object failed";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT: A requested layer is not present";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT: A requested extension is not supported";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT: A requested feature is not supported";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER: The requested Vulkan version is not supported by the driver";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS: Too many objects of a type have been created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED: A requested format is not supported";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL: A descriptor pool allocation has failed due to fragmentation";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY: A descriptor pool allocation has failed due to lack of memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE: An external handle is not valid";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR: The surface is no longer available";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: The native window is already in use";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR: A swapchain no longer matches the surface properties";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR: A swapchain is out of date and must be recreated";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: The display is incompatible with the swapchain";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT: A validation layer has reported an error";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV: A shader is invalid";
        default: return "Unknown VkResult";
    }
}

static VkShaderModule load_shader_module(Context* context, const char* path) {
    auto region = arena_begin_region(&context->scratch);
    defer { region_reset(&region); };
    auto code = read_entire_file_or_crash(&context->scratch, path);

    VkShaderModuleCreateInfo module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.len,
        .pCode = (u32*)code.ptr,
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(context->device, &module_info, nullptr, &shader_module));
    std::println("Created shader module from source {}", path);

    return shader_module;
}

static void create_descriptor_set_layout(Context* context) {
    VkDescriptorSetLayoutBinding ubo_layout = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_layout,
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &context->state.descriptor_set_layout));
}

static void create_descriptor_pool(Context* context) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = max_frames_in_flight,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_frames_in_flight,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    VK_CHECK(vkCreateDescriptorPool(context->device, &pool_info, nullptr, &context->state.descriptor_pool));
}

static void create_descriptor_sets(Context* context) {
    VkDescriptorSetLayout layouts[max_frames_in_flight];
    for (size_t i = 0; i < max_frames_in_flight; i++) {
        layouts[i] = context->state.descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context->state.descriptor_pool,
        .descriptorSetCount = max_frames_in_flight,
        .pSetLayouts = layouts,
    };
    VK_CHECK(vkAllocateDescriptorSets(context->device, &alloc_info, context->state.descriptor_sets));

    for (size_t i = 0; i < max_frames_in_flight; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = context->state.ubo_buffers[i].handle,
            .offset = 0,
            .range = sizeof(UBOMatrices),
        };

        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = context->state.descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };
        vkUpdateDescriptorSets(context->device, 1, &descriptor_write, 0, nullptr);
    }
}

static void traverse_node_hierarchy(Context* context, engine::asset::Node& node, const glm::mat4& parent_transform) {
    auto global_transform = parent_transform * node.transform;

    if (node.kind == engine::asset::Node::Kind::bone) {
        engine::asset::BoneData& bone_data = context->state.bone_data[node.bone_index];
        bone_data.final = global_transform * bone_data.offset;
    } else if (node.kind == engine::asset::Node::Kind::mesh) {
        for (size_t i = 0; i < node.mesh_data.num_meshes; ++i) {
            context->state.mesh_transforms[node.mesh_data.mesh_indices[i]] = global_transform;
        }
    }

    for (size_t i = 0; i < node.child_count; ++i) {
        traverse_node_hierarchy(context, context->state.node_hierarchy[node.child_index + i], global_transform);
    }
}

static void update_bone_transforms(Context* context) {
    auto identity = glm::mat4(1.0f);
    traverse_node_hierarchy(context, context->state.node_hierarchy[0], identity);
}

static void update_uniform_buffers(Context* context, f32 curr_time, const glm::mat4& view_matrix) {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), (f32)context->swapchain.extent.width / (f32)context->swapchain.extent.height, 0.1f, 10000.f);
    proj[1][1] *= -1;

    (void)curr_time;
    UBOMatrices mat_data = {
        .model = glm::translate(context->state.mesh_transforms[0], glm::vec3(0, 0, 0)),
        //.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 5, 0)),
        //.model = glm::scale(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(-1, 0, 0)), glm::vec3(1.0)),
        //.model = glm::translate(glm::rotate(glm::mat4(1.0f), curr_time, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 0.0f, -1.0f)),
        .view = view_matrix,
        .projection = proj,
    };
    memcpy(context->state.ubo_matrices[context->swapchain.image_index], &mat_data, sizeof(UBOMatrices));
}

static void create_uniform_buffers(Context* context) {
    VkDeviceSize buffer_size = sizeof(UBOMatrices);

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        context->state.ubo_buffers[i] = create_buffer(context,
                                                     buffer_size,
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        context->state.ubo_matrices[i] = (UBOMatrices*)context->state.ubo_buffers[i].info.pMappedData;
    }
}

static void destroy_uniform_buffers(Context* context) {
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        auto buf = context->state.ubo_buffers[i];
        vmaDestroyBuffer(context->vma_allocator, buf.handle, buf.allocation);
    }
}

// TODO: Abstract pipeline creation.
static void create_pipeline_skinned(Context* context) {
    auto state = &context->state;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(u32);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &state->descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    };
    VK_CHECK(vkCreatePipelineLayout(context->device, &layout_info, nullptr, &state->skinned_pipeline.layout));

    // Define the vertex input binding description
    VkVertexInputBindingDescription binding_description = {
        .binding   = 0,
        .stride    = sizeof(engine::asset::SkinnedVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    // Define the vertex input attribute descriptions
    std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions = {{
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(engine::asset::SkinnedVertex, pos)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UINT, .offset = offsetof(engine::asset::SkinnedVertex, bone_indices)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(engine::asset::SkinnedVertex, bone_weights)},
    }};

    // Create the vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions    = attribute_descriptions.data()
    };

    // Specify we will use triangle lists to draw geometry.
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Specify rasterization state.
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    // Specify that these states will be dynamic, i.e. not part of pipeline state object.
    std::array<VkDynamicState, 5> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
    };

    // Our attachment will write to all color channels, but no blending is enabled.
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_attachment
    };

    // We will have one viewport and scissor box.
    VkPipelineViewportStateCreateInfo viewport = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    VkCompareOp current_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = current_compare_op,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    // No multisampling.
    VkPipelineMultisampleStateCreateInfo multisample{
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data()
    };

    // Load our SPIR-V shaders.
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {{
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = load_shader_module(context, "shaders/skinning_vert.spv"),
            .pName  = "main"
        },        // Vertex shader stage
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = load_shader_module(context, "shaders/skinning_frag.spv"),
            .pName  = "main"
        }        // Fragment shader stage
    }};

    // Pipeline rendering info (for dynamic rendering).
    VkPipelineRenderingCreateInfo pipeline_rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context->swapchain.surface_format.format,
        .depthAttachmentFormat = context->state.depth_format,
    };

    // Create the graphics pipeline.
    VkGraphicsPipelineCreateInfo pipe{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &pipeline_rendering_info,
        .stageCount          = (u32)shader_stages.size(),
        .pStages             = shader_stages.data(),
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisample,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic_state_info,
        .layout              = state->skinned_pipeline.layout,
        .renderPass          = VK_NULL_HANDLE,                 // Since we are using dynamic rendering this will set as null
        .subpass             = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipe, nullptr, &state->skinned_pipeline.handle));

    // Pipeline is baked, we can delete the shader modules now.
    vkDestroyShaderModule(context->device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(context->device, shader_stages[1].module, nullptr);
}

// From vulkan samples.
static void create_pipeline(Context* context) {
    auto state = &context->state;

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &state->descriptor_set_layout,
    };
    VK_CHECK(vkCreatePipelineLayout(context->device, &layout_info, nullptr, &state->pipeline_layout));

    // Define the vertex input binding description
    VkVertexInputBindingDescription binding_description = {
        .binding   = 0,
        .stride    = sizeof(engine::asset::Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    // Define the vertex input attribute descriptions
    std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions = {{
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(engine::asset::Vertex, pos)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(engine::asset::Vertex, normal)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(engine::asset::Vertex, uv)},
    }};

    // Create the vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions    = attribute_descriptions.data()
    };

    // Specify we will use triangle lists to draw geometry.
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Specify rasterization state.
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    // Specify that these states will be dynamic, i.e. not part of pipeline state object.
    std::array<VkDynamicState, 5> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
    };

    // Our attachment will write to all color channels, but no blending is enabled.
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_attachment
    };

    // We will have one viewport and scissor box.
    VkPipelineViewportStateCreateInfo viewport = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    VkCompareOp current_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = current_compare_op,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    // No multisampling.
    VkPipelineMultisampleStateCreateInfo multisample{
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data()
    };

    // Load our SPIR-V shaders.
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {{
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = load_shader_module(context, "shaders/vert.spv"),
            .pName  = "main"
        },        // Vertex shader stage
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = load_shader_module(context, "shaders/frag.spv"),
            .pName  = "main"
        }        // Fragment shader stage
    }};

    // Pipeline rendering info (for dynamic rendering).
    VkPipelineRenderingCreateInfo pipeline_rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context->swapchain.surface_format.format,
        .depthAttachmentFormat = context->state.depth_format,
    };

    // Create the graphics pipeline.
    VkGraphicsPipelineCreateInfo pipe{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &pipeline_rendering_info,
        .stageCount          = (u32)shader_stages.size(),
        .pStages             = shader_stages.data(),
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisample,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic_state_info,
        .layout              = state->pipeline_layout,        // We need to specify the pipeline layout description up front as well.
        .renderPass          = VK_NULL_HANDLE,                 // Since we are using dynamic rendering this will set as null
        .subpass             = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipe, nullptr, &state->pipeline));

    // Pipeline is baked, we can delete the shader modules now.
    vkDestroyShaderModule(context->device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(context->device, shader_stages[1].module, nullptr);
}

static void copy_buffer(Context* context, VkBuffer dst, VkBuffer src, VkBufferCopy region) {
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->state.pools[0],
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer one_time_buf;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmd_buf_info, &one_time_buf));
    defer { vkFreeCommandBuffers(context->device, context->state.pools[0], 1, &one_time_buf); };

    VkCommandBufferBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK_CHECK(vkBeginCommandBuffer(one_time_buf, &info));
    vkCmdCopyBuffer(one_time_buf, src, dst, 1, &region);
    VK_CHECK(vkEndCommandBuffer(one_time_buf));

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &one_time_buf,
    };

    // Just wait until the queue is done and avoid using fences.
    VK_CHECK(vkQueueSubmit(context->graphics_queue.handle, 1, &submit_info, nullptr));
    VK_CHECK(vkQueueWaitIdle(context->graphics_queue.handle));
}

static Slice<u8> read_entire_file_or_crash(Arena* arena, const char* path) {
    auto file = platform::open_file(path);
    if (file.handle == nullptr) {
        platform::fatal(std::format("Failed to open file at path {}", path));
    }
    defer { platform::close_file(file); };

    auto file_size = platform::get_file_size(file);
    assert(file_size != 0);

    auto buf = arena_alloc_aligned<u8>(arena, file_size, alignof(u64));
    auto size = platform::read_from_file(file, buf);
    if (size != file_size) {
        platform::fatal(std::format("Failed to read entire file at path {}", path));
    }

    return buf;
}

static Buffer create_buffer(Context* context,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VmaAllocationCreateFlags vma_flags) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = vma_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Buffer buffer;
    VK_CHECK(vmaCreateBuffer(context->vma_allocator, &buffer_info, &allocation_info, &buffer.handle, &buffer.allocation, &buffer.info));
    return buffer;
}

static void upload_vertices(Context* context, Buffer staging, const engine::asset::Header& header) {
    // Static vertices
    VkDeviceSize static_vertex_buffer_size = sizeof(engine::asset::Vertex) * header.num_static_vertices;
    VkDeviceSize skinned_vertex_buffer_size = sizeof(engine::asset::SkinnedVertex) * header.num_skinned_vertices;
    if (header.num_static_meshes != 0) {
        context->state.static_vertex_buffer = create_buffer(
            context,
            static_vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0
        );
        VkBufferCopy region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = static_vertex_buffer_size,
        };
        copy_buffer(context, context->state.static_vertex_buffer.handle, staging.handle, region);
    }
    // Skinned vertices
    if (header.num_skinned_meshes != 0) {
        context->state.skinned_vertex_buffer = create_buffer(
            context,
            skinned_vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0
        );
        VkBufferCopy region = {
            .srcOffset = static_vertex_buffer_size,
            .dstOffset = 0,
            .size = skinned_vertex_buffer_size,
        };
        copy_buffer(context, context->state.skinned_vertex_buffer.handle, staging.handle, region);
    }
}

static void upload_indices(Context* context, Buffer staging, const engine::asset::Header& header) {
    VkDeviceSize buffer_size = sizeof(u16) * header.num_indices;
    context->state.index_buffer = create_buffer(
        context,
        buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        0
    );

    VkBufferCopy region = {
        .srcOffset = engine::asset::get_index_asset_offset(header),
        .dstOffset = 0,
        .size = buffer_size,
    };
    copy_buffer(context, context->state.index_buffer.handle, staging.handle, region);
}

static void load_meshes(Context* context, const engine::asset::Header& header, platform::OSHandle file) {
    context->state.static_meshes = arena_alloc<engine::asset::StaticMesh>(&context->scratch, header.num_static_meshes);
    context->state.skinned_meshes = arena_alloc<engine::asset::SkinnedMesh>(&context->scratch, header.num_skinned_meshes);
    context->state.mesh_transforms = arena_alloc<glm::mat4>(&context->scratch, header.num_static_meshes + header.num_skinned_meshes);

    // Static
    {
        auto to_read = header.num_static_meshes * sizeof(engine::asset::StaticMesh);
        auto size = platform::read_from_file(file, { .ptr = (u8*)context->state.static_meshes.ptr, .len = to_read } );
        if (size != to_read) {
            platform::fatal("Failed to read mesh metadata");
        }
    }
    // Skinned
    {
        auto to_read = header.num_skinned_meshes * sizeof(engine::asset::SkinnedMesh);
        auto size = platform::read_from_file(file, { .ptr = (u8*)context->state.skinned_meshes.ptr, .len = to_read } );
        if (size != to_read) {
            platform::fatal("Failed to read mesh metadata");
        }
    }
}

static void load_node_hierarchy(Context* context, const engine::asset::Header& header, platform::OSHandle file) {
    context->state.node_hierarchy = arena_alloc<engine::asset::Node>(&context->scratch, header.num_nodes);
    auto to_read = header.num_nodes * sizeof(engine::asset::Node);
    auto size = platform::read_from_file(file, { .ptr = (u8*)context->state.node_hierarchy.ptr, .len = to_read } );
    if (size != to_read) {
        platform::fatal("Failed to read node hierarchy");
    }
}


static void load_mesh_data(Context* context) {
    const char* path = "tools/asset_processor/mesh_data_new.bin";

    // Open file
    auto file = platform::open_file(path);
    if (file.handle == nullptr) {
        platform::fatal(std::format("Failed to open file at path {}", path));
    }
    defer { platform::close_file(file); };

    // read asset header to CPU memory.
    engine::asset::Header header;
    auto size = platform::read_from_file(file, { .ptr = (u8*)&header, .len = sizeof(engine::asset::Header) });
    if (size != sizeof(engine::asset::Header)) {
        platform::fatal("Failed to read asset header");
    }

    load_meshes(context, header, file);
    load_node_hierarchy(context, header, file);

    auto file_size = platform::get_file_size(file);
    assert(file_size != 0);

    VkDeviceSize staging_size = engine::asset::total_size_of_assets(header);
    u64 metadata_size = engine::asset::metadata_size(header);
    if (staging_size + metadata_size != file_size) {
        platform::fatal("Invalid size of asset file");
    }

    // Allocate staging buffer
    Buffer staging = create_buffer(context,
                                   file_size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    defer { vmaDestroyBuffer(context->vma_allocator, staging.handle, staging.allocation); };

    Slice<u8> buf = {
        .ptr = (u8*)staging.info.pMappedData,
        .len = file_size,
    };

    size = platform::read_from_file(file, buf);
    if (size != staging_size) {
        platform::fatal(std::format("Failed to read entire file at path {}", path));
    }

    upload_vertices(context, staging, header);
    upload_indices(context, staging, header);
}

// From vulkan samples.
static void transition_image_layout(
    VkCommandBuffer       cmd,
    VkImage               image,
    VkImageLayout         oldLayout,
    VkImageLayout         newLayout,
    VkAccessFlags2        srcAccessMask,
    VkAccessFlags2        dstAccessMask,
    VkPipelineStageFlags2 srcStage,
    VkPipelineStageFlags2 dstStage)
{
    bool is_depth_attachment = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    // Initialize the VkImageMemoryBarrier2 structure
    VkImageMemoryBarrier2 image_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        // Specify the pipeline stages and access masks for the barrier
        .srcStageMask  = srcStage,             // Source pipeline stage mask
        .srcAccessMask = srcAccessMask,        // Source access mask
        .dstStageMask  = dstStage,             // Destination pipeline stage mask
        .dstAccessMask = dstAccessMask,        // Destination access mask

        // Specify the old and new layouts of the image
        .oldLayout = oldLayout,        // Current layout of the image
        .newLayout = newLayout,        // Target layout of the image

        // We are not changing the ownership between queues
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        // Specify the image to be affected by this barrier
        .image = image,

        // Define the subresource range (which parts of the image are affected)
        .subresourceRange = {
            .aspectMask     = is_depth_attachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,                                // Start at mip level 0
            .levelCount     = 1,                                // Number of mip levels affected
            .baseArrayLayer = 0,                                // Start at array layer 0
            .layerCount     = 1                                 // Number of array layers affected
    }};

    // Initialize the VkDependencyInfo structure
    VkDependencyInfo dependency_info{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags         = 0,                    // No special dependency flags
        .imageMemoryBarrierCount = 1,                    // Number of image memory barriers
        .pImageMemoryBarriers    = &image_barrier        // Pointer to the image memory barrier(s)
    };

    // Record the pipeline barrier into the command buffer
    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

static void create_command_pool_and_buffers(Context* context) {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = context->graphics_queue.family,
    };

    for (u32 i = 0; i < context->swapchain.frames_in_flight; ++i) {
         VK_CHECK(vkCreateCommandPool(context->device, &cmd_pool_create_info, nullptr, &context->state.pools[i]));
         VkCommandBufferAllocateInfo cmd_buf_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = context->state.pools[i],
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
         VK_CHECK(vkAllocateCommandBuffers(context->device, &cmd_buf_info, &context->state.cmd_bufs[i]));
    }
}

static void destroy_command_pools(Context* context) {
    for (u32 i = 0; i < context->swapchain.frames_in_flight; ++i) {
        vkDestroyCommandPool(context->device, context->state.pools[i], nullptr);
    }
}

static void render_mesh(Context* context) {
    u32 current_image_index = context->swapchain.image_index;
    auto cmd = context->state.cmd_bufs[current_image_index];

    VkClearValue clear_value = {
        .color = { { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = (f32)context->swapchain.extent.width,
        .height = (f32)context->swapchain.extent.height,
        .minDepth = 1.0f,
        .maxDepth = 0.0f,
    };

    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = context->swapchain.extent,
    };

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    transition_image_layout(
        cmd,
        context->swapchain.images[current_image_index].image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,                                                     // srcAccessMask (no need to wait for previous operations)
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,                // dstAccessMask
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,                   // srcStage
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT        // dstStage
    );

    transition_image_layout(
        cmd,
        context->state.depth_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        0,                                                     // srcAccessMask (no need to wait for previous operations)
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,        // dstAccessMask
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,                   // srcStage
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT          // dstStage
    );

    // Set up the rendering attachment info
    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context->swapchain.images[current_image_index].view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_value,
    };

    // Depth attachment
    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context->state.depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .depthStencil = { .depth = 0.0f } },
    };

    // Begin rendering
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = {
                .width  = context->swapchain.extent.width,
                .height = context->swapchain.extent.height
            }
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment  = &depth_attachment,
    };

    bool draw_skinned_mesh = context->state.static_meshes.len == 0;

    vkCmdBeginRendering(cmd, &rendering_info);

    // Bind the graphics pipeline.
    if (draw_skinned_mesh) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->state.skinned_pipeline.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->state.skinned_pipeline.layout, 0, 1, &context->state.descriptor_sets[current_image_index], 0, nullptr);
        vkCmdPushConstants(cmd, context->state.skinned_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(u32), &context->state.display_bone);
    } else {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->state.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context->state.pipeline_layout, 0, 1, &context->state.descriptor_sets[current_image_index], 0, nullptr);
    }

    // Set dynamic states
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Since we declared VK_DYNAMIC_STATE_CULL_MODE as dynamic in the pipeline,
    // we need to set the cull mode here. VK_CULL_MODE_NONE disables face culling,
    // meaning both front and back faces will be rendered.
    vkCmdSetCullMode(cmd, VK_CULL_MODE_FRONT_BIT);

    // Since we declared VK_DYNAMIC_STATE_FRONT_FACE as dynamic,
    // we need to specify the winding order considered as the front face.
    // VK_FRONT_FACE_CLOCKWISE indicates that vertices defined in clockwise order
    // are considered front-facing.
    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);

    // Since we declared VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY as dynamic,
    // we need to set the primitive topology here. VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    // tells Vulkan that the input vertex data should be interpreted as a list of triangles.
    vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    // Bind the vertex buffer
    VkDeviceSize offset = {0};
    vkCmdBindIndexBuffer(cmd, context->state.index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);
    if (draw_skinned_mesh) {
        vkCmdBindVertexBuffers(cmd, 0, 1, &context->state.skinned_vertex_buffer.handle, &offset);
        vkCmdDrawIndexed(cmd, context->state.skinned_meshes[0].num_indices, 1, context->state.skinned_meshes[0].base_index, context->state.skinned_meshes[0].base_vertex, 0);
    } else {
        vkCmdBindVertexBuffers(cmd, 0, 1, &context->state.static_vertex_buffer.handle, &offset);
        vkCmdDrawIndexed(cmd, context->state.static_meshes[0].num_indices, 1, context->state.static_meshes[0].base_index, context->state.static_meshes[0].base_vertex, 0);
    }

    // Complete rendering.
    vkCmdEndRendering(cmd);

    // After rendering , transition the swapchain image to PRESENT_SRC
    transition_image_layout(
        cmd,
        context->swapchain.images[current_image_index].image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,                 // srcAccessMask
        0,                                                      // dstAccessMask
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,        // srcStage
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT                  // dstStage
    );

    VK_CHECK(vkEndCommandBuffer(cmd));
}

static void create_depth_resources(Context* context) {
    context->state.depth_format = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = {
            .width = context->swapchain.extent.width,
            .height = context->swapchain.extent.height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo allocation_info = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VK_CHECK(vmaCreateImage(context->vma_allocator, &image_create_info, &allocation_info, &context->state.depth_image, &context->state.depth_image_allocation, nullptr));

    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .image = context->state.depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_create_info.format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VK_CHECK(vkCreateImageView(context->device, &view_create_info, nullptr, &context->state.depth_image_view));
}

static void destroy_depth_resources(Context* context) {
    vkDestroyImageView(context->device, context->state.depth_image_view, nullptr);
    vmaDestroyImage(context->vma_allocator, context->state.depth_image, context->state.depth_image_allocation);
}

static void create_texture_image(Context* context) {
    (void)context;
}

void create_state_for_triangle(Context* context) {
    create_depth_resources(context);
    std::println("Created depth buffer");

    create_uniform_buffers(context);
    std::println("Created uniform buffers");

    create_descriptor_set_layout(context);
    std::println("Created descriptor set layout");

    create_descriptor_pool(context);
    std::println("Created descriptor pool");

    create_descriptor_sets(context);
    std::println("Created descriptor sets");

    create_pipeline(context);
    std::println("Created pipeline");

    create_pipeline_skinned(context);
    std::println("Created skinned pipeline");

    create_command_pool_and_buffers(context);
    std::println("Created command pool");

    load_mesh_data(context);
    std::println("Created and uploaded mesh data");

    traverse_node_hierarchy(context, context->state.node_hierarchy[0], glm::mat4(1.0f));
    std::println("Updated transforms. TODO: Do this every frame");
}

static void destroy_temp_state(Context* context) {
    std::println("Destroying temp state");

    // Depth buffer.
    destroy_depth_resources(context);

    // Vertex buffers
    vmaDestroyBuffer(context->vma_allocator, context->state.static_vertex_buffer.handle, context->state.static_vertex_buffer.allocation);
    vmaDestroyBuffer(context->vma_allocator, context->state.skinned_vertex_buffer.handle, context->state.skinned_vertex_buffer.allocation);

    // Index buffer
    vmaDestroyBuffer(context->vma_allocator, context->state.index_buffer.handle, context->state.index_buffer.allocation);

    // Command pools
    destroy_command_pools(context);

    // Descriptor pool
    vkDestroyDescriptorPool(context->device, context->state.descriptor_pool, nullptr);

    // Descriptor set layout
    vkDestroyDescriptorSetLayout(context->device, context->state.descriptor_set_layout, nullptr);

    // Uniform buffers.
    destroy_uniform_buffers(context);

    // Pipeline
    vkDestroyPipeline(context->device, context->state.pipeline, nullptr);
    vkDestroyPipelineLayout(context->device, context->state.pipeline_layout, nullptr);

    // Skinned Pipeline
    vkDestroyPipeline(context->device, context->state.skinned_pipeline.handle, nullptr);
    vkDestroyPipelineLayout(context->device, context->state.skinned_pipeline.layout, nullptr);
    std::println("Destroyed temp state");
}

}

