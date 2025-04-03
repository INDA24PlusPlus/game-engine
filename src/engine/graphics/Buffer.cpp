#include "Buffer.h"

#include "VmaUsage.h"
#include "context.h"

namespace renderer {

Buffer Buffer::init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vma_flags) {
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
    VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &allocation_info, &buffer.handle, &buffer.allocation, &buffer.info));
    return buffer;
}

void Buffer::deinit(VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, handle, allocation);
}

}