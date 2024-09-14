#include "Buffer.h"

Buffer::Buffer(VkDevice device, VmaAllocator allocator, const VkBufferCreateInfo &bufferInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &name) : Resource(device, name), allocator(allocator), size(bufferInfo.size) {
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
}

Buffer::~Buffer() {
    destroy();
}

void Buffer::destroy() {
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

void* Buffer::map() {
    void *mappedData;
    vmaMapMemory(allocator, allocation, &mappedData);
    return mappedData;
}

void Buffer::unmap() {
    vmaUnmapMemory(allocator, allocation);
}

void Buffer::flush(VkDeviceSize offset, VkDeviceSize size) {
    vmaFlushAllocation(allocator, allocation, offset, size);
}