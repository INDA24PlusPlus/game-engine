#include "context.h"
#include "engine/assets.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_common.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "vulkan/vulkan_core.h"

#include <print>
#include <optional>
#include <winnt.h>

namespace renderer {

void Context::init(const char* app_name, u32 fb_width, u32 fb_height, platform::WindowInfo window_info) {
    std::println("Initializing Vulkan context");
    VK_CHECK(volkInitialize());
    std::println("Loaded Vulkan procedures");

    if (enable_validation_layers && !check_validation_support(this)) {
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

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &instance));
    volkLoadInstance(instance);
    std::println("Created Vulkan instance");

    surface = create_surface(instance, window_info);
    std::println("Created Vulkan surface");

    auto physical_device = pick_physical_device(instance, surface, &scratch);
    pdev = physical_device.pdev;
    pdev_props = physical_device.props;
    std::println("Selected device {}", physical_device.props.deviceName);

    device = init_physical_device(&physical_device);
    volkLoadDevice(device);
    std::println("Created device interface");

    vkGetDeviceQueue(device, physical_device.queues.graphics_family, 0, &graphics_queue.handle);
    graphics_queue.family = physical_device.queues.graphics_family;

    create_vma_allocator(this);

    // NOTE: FIFO is always supported. When chaning this use 'is_present_mode_supported' before specifying anything else.
    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkSurfaceFormatKHR preferred_swapchain_surface_format = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    VkSurfaceFormatKHR swapchain_surface_format = find_surface_format(this, preferred_swapchain_surface_format);

    // FIXME: hardcoded to 2 frames in flight.
    u32 frames_in_flight = 2;
    swapchain.init(this, { .width = fb_width, .height = fb_height }, nullptr, swapchain_surface_format, swapchain_present_mode, frames_in_flight);
    state.init(this);
}

void Context::deinit() {
    std::println("Destroying context...");
    vkDeviceWaitIdle(device);

    if (swapchain.m_handle != nullptr) {
        swapchain.deinit(this);
    }

    state.deinit(this);
    vmaDestroyAllocator(vma_allocator); 

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    std::println("Destroyed context");
}

void Context::allocate_descriptor_sets(Slice<VkDescriptorSetLayout> layouts, Slice<VkDescriptorSet> sets) {
    assert(layouts.len == sets.len);

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state.m_descriptor_pool,
        .descriptorSetCount = (u32)sets.len,
        .pSetLayouts = layouts.ptr,
    };

    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, sets.ptr));
}

VkShaderModule Context::load_shader_module(const char* path) {
    auto region = arena_begin_region(&scratch);
    defer { region_reset(&region); };
    auto code = read_entire_file_or_crash(&scratch, path);

    VkShaderModuleCreateInfo module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.len,
        .pCode = (u32*)code.ptr,
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &module_info, nullptr, &shader_module));
    std::println("Created shader module from source {}", path);

    return shader_module;
}

VkPipeline Context::build_pipeline(VkPipelineLayout layout, Slice<VkVertexInputAttributeDescription> vertex_attributes, u32 vertex_stride, VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
    // Define the vertex input binding description
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = vertex_stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    // Create the vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = (u32)vertex_attributes.len,
        .pVertexAttributeDescriptions = vertex_attributes.ptr,
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment
    };

    // We will have one viewport and scissor box.
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkCompareOp current_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
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
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (u32)dynamic_states.size(),
        .pDynamicStates = dynamic_states.data()
    };

    // Load our SPIR-V shaders.
    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {{
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName  = "main"
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName  = "main"
        }
    }};

    // Pipeline rendering info (for dynamic rendering).
    VkPipelineRenderingCreateInfo pipeline_rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain.m_surface_format.format,
        .depthAttachmentFormat = state.m_depth_format,
    };

    // Create the graphics pipeline.
    VkGraphicsPipelineCreateInfo pipe = {
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
        .layout              = layout,
        .renderPass          = VK_NULL_HANDLE,                 // Since we are using dynamic rendering this will set as null
        .subpass             = 0,
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &pipeline));

    // Pipeline is baked, we can delete the shader modules now.
    vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
    return pipeline;
}


void RenderState::init(Context* context) {
    init_depth_resources(context);
    std::println("Created depth buffer");

    init_descriptor_pool(context);
    std::println("Created descriptor pool");

    init_command_pool_and_buffers(context);
    std::println("Created command pool");

    load_mesh_data(context);
    std::println("Created and uploaded mesh data");

    traverse_node_hierarchy(context, context->state.node_hierarchy[0], glm::mat4(1.0f), 0.0);
    std::println("Updated transforms. TODO: Do this every frame");

    m_skinned_mesh_pass.init(context, context->state.m_skinned_vertex_buffer, bone_data.len);
}

void RenderState::deinit(Context* context) {
    deinit_depth_resources(context);
    deinit_descriptor_pool(context);
    deinit_command_pool_and_buffers(context);

    m_skinned_vertex_buffer.deinit(context->vma_allocator);
    m_static_vertex_buffer.deinit(context->vma_allocator);
    m_index_buffer.deinit(context->vma_allocator);

    m_skinned_mesh_pass.deinit(context);
}

void RenderState::init_depth_resources(Context* context) {
    m_depth_format = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = {
            .width = context->swapchain.m_extent.width,
            .height = context->swapchain.m_extent.height,
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

    VK_CHECK(vmaCreateImage(context->vma_allocator, &image_create_info, &allocation_info, &m_depth_image, &m_depth_image_allocation, nullptr));

    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .image = m_depth_image,
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
    VK_CHECK(vkCreateImageView(context->device, &view_create_info, nullptr, &m_depth_image_view));
}

void RenderState::deinit_depth_resources(Context* context) {
    vkDestroyImageView(context->device, m_depth_image_view, nullptr);
    vmaDestroyImage(context->vma_allocator, m_depth_image, m_depth_image_allocation);
}

void RenderState::init_descriptor_pool(Context* context) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 2 * max_frames_in_flight,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2 * max_frames_in_flight,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    VK_CHECK(vkCreateDescriptorPool(context->device, &pool_info, nullptr, &m_descriptor_pool));
}

void RenderState::deinit_descriptor_pool(Context* context) {
    vkDestroyDescriptorPool(context->device, m_descriptor_pool, nullptr);
}

void RenderState::init_command_pool_and_buffers(Context* context) {
    VkCommandPoolCreateInfo cmd_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = context->graphics_queue.family,
    };

    for (u32 i = 0; i < context->swapchain.m_frames_in_flight; ++i) {
        VK_CHECK(vkCreateCommandPool(context->device, &cmd_pool_create_info, nullptr, &m_pools[i]));
         VkCommandBufferAllocateInfo cmd_buf_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_pools[i],
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(context->device, &cmd_buf_info, &m_cmd_bufs[i]));
    }
}

void RenderState::deinit_command_pool_and_buffers(Context* context) {
    for (u32 i = 0; i < context->swapchain.m_frames_in_flight; ++i) {
        vkDestroyCommandPool(context->device, m_pools[i], nullptr);
    }
}

void RenderState::record_commands(Context* context) {
    u32 current_image_index = context->swapchain.m_image_index;
    auto cmd = m_cmd_bufs[current_image_index];

    VkClearValue clear_value = {
        .color = { { 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = (f32)context->swapchain.m_extent.width,
        .height = (f32)context->swapchain.m_extent.height,
        .minDepth = 1.0f,
        .maxDepth = 0.0f,
    };

    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = context->swapchain.m_extent,
    };

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    transition_image_layout(
        cmd,
        context->swapchain.m_images[current_image_index].m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,                                                     // srcAccessMask (no need to wait for previous operations)
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,                // dstAccessMask
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,                   // srcStage
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT        // dstStage
    );

    transition_image_layout(
        cmd,
        context->state.m_depth_image,
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
        .imageView = context->swapchain.m_images[current_image_index].m_view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_value,
    };

    // Depth attachment
    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context->state.m_depth_image_view,
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
                .width  = context->swapchain.m_extent.width,
                .height = context->swapchain.m_extent.height
            }
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment  = &depth_attachment,
    };

    vkCmdBeginRendering(cmd, &rendering_info);

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

    vkCmdBindIndexBuffer(cmd, context->state.m_index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);
    m_skinned_mesh_pass.render(cmd, context->state.skinned_meshes, current_image_index, context->state.display_bone);

    // Complete rendering.
    vkCmdEndRendering(cmd);

    // After rendering , transition the swapchain image to PRESENT_SRC
    transition_image_layout(
        cmd,
        context->swapchain.m_images[current_image_index].m_image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,                 // srcAccessMask
        0,                                                      // dstAccessMask
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,        // srcStage
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT                  // dstStage
    );

    VK_CHECK(vkEndCommandBuffer(cmd));
}

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
            .aspectMask     = is_depth_attachment ? (u32)VK_IMAGE_ASPECT_DEPTH_BIT : (u32)VK_IMAGE_ASPECT_COLOR_BIT,
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

static void upload_vertices(Context* context, Buffer staging, engine::asset::AssetLoader& loader) {
    // Static vertices
    VkDeviceSize static_vertex_buffer_size = loader.static_vertices_size();
    VkDeviceSize skinned_vertex_buffer_size = loader.skinned_vertices_size();
    if (static_vertex_buffer_size != 0) {
        context->state.m_static_vertex_buffer = Buffer::init(
            context->vma_allocator,
            static_vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0
        );
        VkBufferCopy region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = static_vertex_buffer_size,
        };
        copy_buffer(context, context->state.m_static_vertex_buffer.handle, staging.handle, region);
    }

    // Skinned vertices
    if (skinned_vertex_buffer_size != 0) {
        context->state.m_skinned_vertex_buffer = Buffer::init(
            context->vma_allocator,
            skinned_vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0
        );
        VkBufferCopy region = {
            .srcOffset = static_vertex_buffer_size,
            .dstOffset = 0,
            .size = skinned_vertex_buffer_size,
        };
        copy_buffer(context, context->state.m_skinned_vertex_buffer.handle, staging.handle, region);
    }
}

static void upload_indices(Context* context, Buffer staging, engine::asset::AssetLoader& loader) {
    VkDeviceSize buffer_size = loader.indices_size();
    context->state.m_index_buffer = Buffer::init(
        context->vma_allocator,
        buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        0
    );

    VkBufferCopy region = {
        .srcOffset = loader.offset_to_indics(),
        .dstOffset = 0,
        .size = buffer_size,
    };
    copy_buffer(context, context->state.m_index_buffer.handle, staging.handle, region);
}

static void load_mesh_data(Context* context) {
    const char* path = "tools/asset_processor/mesh_data_new.bin";

    engine::asset::AssetLoader loader;
    loader.init(path);
    defer { loader.deinit(); };

    auto pack = loader.load_cpu_data(&context->scratch);
    context->state.static_meshes = pack.static_meshes;
    context->state.skinned_meshes = pack.skinned_meshes;
    context->state.node_hierarchy = pack.nodes;
    context->state.bone_data = pack.bone_data;

    context->state.global_inverse_matrices = pack.global_inverse_matrices;
    context->state.animation_data = pack.animations;
    context->state.animation_channels = pack.animation_channels;
    context->state.animation_translations = pack.translation_keys;
    context->state.animation_scalings = pack.scaling_keys;
    context->state.animation_rotations = pack.rotation_keys;

    context->state.mesh_transforms = arena_alloc<glm::mat4>(&context->scratch, loader.header.num_static_meshes + loader.header.num_skinned_meshes);


    auto staging_size = loader.gpu_data_size();
    Buffer staging = Buffer::init(context->vma_allocator,
                                  staging_size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    defer { staging.deinit(context->vma_allocator); };

    Slice<u8> staging_buffer = { .ptr = (u8*)staging.info.pMappedData, .len = staging_size };
    loader.load_gpu_data(staging_buffer);

    upload_vertices(context, staging, loader);
    upload_indices(context, staging, loader);
}

static void copy_buffer(Context* context, VkBuffer dst, VkBuffer src, VkBufferCopy region) {
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->state.m_pools[0],
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer one_time_buf;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &cmd_buf_info, &one_time_buf));
    defer { vkFreeCommandBuffers(context->device, context->state.m_pools[0], 1, &one_time_buf); };

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

static glm::vec3 lerp_animation_translation(Context* context, const engine::asset::NodeAnim& anim, f32 time_in_ticks) {
    auto translation_keys = context->state.animation_translations;
    if (anim.num_translations == 1) {
        return translation_keys[anim.translation_index].translation;
    }

    u32 key_index = 0;
    for (size_t i = 0; i < anim.num_translations - 1; ++i) {
        if (time_in_ticks < translation_keys[anim.translation_index + i].time) {
            key_index = i + anim.translation_index;
        }
    }

    const auto& k1 = translation_keys[key_index];
    const auto& k2 = translation_keys[key_index + 1];
    f32 delta_time = k2.time - k1.time;
    f32 factor = (time_in_ticks - k1.time) / delta_time;
    return glm::mix(k1.translation, k2.translation, factor);
}

static glm::vec3 lerp_animation_scale(Context* context, const engine::asset::NodeAnim& anim, f32 time_in_ticks) {
    auto scale_keys = context->state.animation_scalings;
    if (anim.num_scalings == 1) {
        return scale_keys[anim.scaling_index].scale;
    }

    u32 key_index = 0;
    for (size_t i = 0; i < anim.num_scalings - 1; ++i) {
        if (time_in_ticks < scale_keys[anim.scaling_index + i].time) {
            key_index = i + anim.scaling_index;
        }
    }

    const auto& k1 = scale_keys[key_index];
    const auto& k2 = scale_keys[key_index + 1];
    f32 delta_time = k2.time - k1.time;
    f32 factor = (time_in_ticks - k1.time) / delta_time;
    return glm::mix(k1.scale, k2.scale, factor);
}


static glm::quat lerp_animation_rotation(Context* context, const engine::asset::NodeAnim& anim, f32 time_in_ticks) {
    auto rotation_keys = context->state.animation_rotations;
    if (anim.num_scalings == 1) {
        return rotation_keys[anim.rotation_index].rotation;
    }

    u32 key_index = 0;
    for (size_t i = 0; i < anim.num_rotations - 1; ++i) {
        if (time_in_ticks < rotation_keys[anim.rotation_index + i].time) {
            key_index = i + anim.rotation_index;
        }
    }

    const auto& k1 = rotation_keys[key_index];
    const auto& k2 = rotation_keys[key_index + 1];
    f32 delta_time = k2.time - k1.time;
    f32 factor = (time_in_ticks - k1.time) / delta_time;
    auto ret =  glm::slerp(k1.rotation, k2.rotation, factor);
    return glm::normalize(ret);
}

static void traverse_node_hierarchy(Context* context, engine::asset::Node& node, const glm::mat4& parent_transform, f32 curr_time) {
    auto global_transform = parent_transform * node.transform;

    if (node.kind == engine::asset::Node::Kind::bone) { 
        // For now we only do the first animation.
        const auto& animation = context->state.animation_data[0];
        engine::asset::BoneData& bone_data = context->state.bone_data[node.bone_index];

        f32 time_in_ticks = curr_time * animation.ticks_per_second;
        //f32 animation_time = fmodf(time_in_ticks, animation.duration);
        f32 animation_time = 0.0f;

        const auto& channel = context->state.animation_channels[node.bone_index];
        glm::mat4 node_transform = node.transform;

        //if (!channel.is_identity) {
        //    auto translation = lerp_animation_translation(context, channel, animation_time);
        //    auto scale = lerp_animation_scale(context, channel, animation_time);
        //    auto rotation = lerp_animation_rotation(context, channel, animation_time);
        //    node_transform = glm::translate(glm::mat4(1.0f), translation);
        //}

        bone_data.final = context->state.global_inverse_matrices[0] * parent_transform * node_transform * bone_data.offset;
    }

    if (node.kind == engine::asset::Node::Kind::mesh) {
        for (size_t i = 0; i < node.mesh_data.num_meshes; ++i) {
            context->state.mesh_transforms[node.mesh_data.mesh_indices[i]] = global_transform;
        }
    }

    for (size_t i = 0; i < node.child_count; ++i) {
        traverse_node_hierarchy(context, context->state.node_hierarchy[node.child_index + i], global_transform, curr_time);
    }
}

};