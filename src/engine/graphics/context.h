#ifndef _GRAPHICS_CONTEXT
#define _GRAPHICS_CONTEXT

#include <array>

#include "../arena.h"
#include "../core.h"
#include "../platform.h"

#include "glm/fwd.hpp"
#include "vk.h"
#include "Swapchain.h"
#include "SkinnedMeshPass.h"
#include "vulkan/vulkan_core.h"

namespace renderer {

#define VK_CHECK(f)                                                                              \
    do {                                                                                         \
        VkResult res = (f);                                                                      \
        if (res != VK_SUCCESS) {                                                                 \
            platform::fatal(std::format("Fatal Vulkan error: {}", vk_result_to_string(res)));    \
        }                                                                                        \
    } while (0)


struct QueueAllocation {
    u32 graphics_family; // NOTE: For now we require the graphics queue to support present operations.
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

struct RenderState {
    SkinnedMeshPass m_skinned_mesh_pass;

    VkDescriptorPool m_descriptor_pool;

    Buffer m_static_vertex_buffer;
    Buffer m_skinned_vertex_buffer;
    Buffer m_index_buffer;

    VkCommandPool m_pools[max_frames_in_flight];
    VkCommandBuffer m_cmd_bufs[max_frames_in_flight];

    VkImage m_depth_image;
    VmaAllocation m_depth_image_allocation;
    VkImageView m_depth_image_view;
    VkFormat m_depth_format;

    // TODO: Pull this out in to a scene abstraction.
    Slice<glm::mat4> mesh_transforms;
    Slice<engine::asset::StaticMesh> static_meshes;
    Slice<engine::asset::SkinnedMesh> skinned_meshes;

    Slice<engine::asset::Node> node_hierarchy;
    Slice<engine::asset::BoneData> bone_data;
    Slice<glm::mat4> global_inverse_matrices;

    // Animation data
    Slice<engine::asset::Animation> animation_data;
    Slice<engine::asset::NodeAnim> animation_channels;
    Slice<engine::asset::TranslationKey> animation_translations;
    Slice<engine::asset::ScaleKey> animation_scalings;
    Slice<engine::asset::RotationKey> animation_rotations;


    u32 display_bone;

    void init(Context* context);
    void deinit(Context* context);

    void init_depth_resources(Context* context);
    void deinit_depth_resources(Context* context);

    void init_descriptor_pool(Context* context);
    void deinit_descriptor_pool(Context* context);

    void init_command_pool_and_buffers(Context* context);
    void deinit_command_pool_and_buffers(Context* context);

    void record_commands(Context* context);
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
    RenderState state;

    Arena scratch;

    void init(const char* app_name, u32 fb_width, u32 fb_height, platform::WindowInfo window_info);
    void deinit();

    void allocate_descriptor_sets(Slice<VkDescriptorSetLayout> layouts, Slice<VkDescriptorSet> sets);
    VkShaderModule load_shader_module(const char* path);
    VkPipeline build_pipeline(VkPipelineLayout layout, Slice<VkVertexInputAttributeDescription> vertex_attributes, u32 vertex_stride, VkShaderModule vertex_shader, VkShaderModule fragment_shader);
};

#ifdef _WIN32
constexpr std::array<const char* const, 2> instance_extensions = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface"
};
#endif

constexpr std::array<const char* const, 1> required_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr std::array<const char*, 1> validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr bool enable_validation_layers = true;
constexpr uint32_t num_validation_layers = enable_validation_layers ? validation_layers.size() : 0;

static VkSurfaceKHR create_surface(VkInstance instance, platform::WindowInfo window_info);
static PhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, Arena* scratch);
static VkDevice init_physical_device(const PhysicalDevice* pdev);
static void create_vma_allocator(Context* context);
static VkSurfaceFormatKHR find_surface_format(Context* context, VkSurfaceFormatKHR preferred);
static bool check_validation_support(Context* context);
static Slice<u8> read_entire_file_or_crash(Arena* arena, const char* path);
static void copy_buffer(Context* context, VkBuffer dst, VkBuffer src, VkBufferCopy region);
static void load_mesh_data(Context* context);
static void traverse_node_hierarchy(Context* context, engine::asset::Node& node, const glm::mat4& parent_transform, f32 curr_time);
static void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout,
                                    VkAccessFlags2 srcAccessMask,
                                    VkAccessFlags2 dstAccessMask,
                                    VkPipelineStageFlags2 srcStage,
                                    VkPipelineStageFlags2 dstStage);

static const char* vk_result_to_string(VkResult result) {
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

}

#endif
