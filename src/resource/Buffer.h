#pragma once
#include "util/VulkanUtils.h"

#include "Resource.h"
#include "MemoryAllocator.h"

class Buffer : public Resource {
public:
    Buffer(VkDevice device, VmaAllocator allocator, const VkBufferCreateInfo &bufferInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &name);
    ~Buffer() override;

    void destroy() override;

    VkBuffer getBuffer() const { return buffer; }
    VmaAllocation getAllocation() const { return allocation; }
    VkDeviceSize getSize() const { return size; }

    void *map();
    void unmap();
    void flush();

private:
    VmaAllocator allocator;
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};
