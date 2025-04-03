#ifndef _BUFFER_H
#define _BUFFER_H

#include "vk.h"

namespace renderer {

struct Buffer {
    VkBuffer handle;
    VmaAllocation allocation;
    VmaAllocationInfo info;

    static Buffer init(VmaAllocator context, VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VmaAllocationCreateFlags vma_flags);
    void deinit(VmaAllocator allocator);
};

}

#endif