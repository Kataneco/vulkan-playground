#pragma once
#include "util/VulkanUtils.h"

#include "core/VulkanInstance.h"
#include "core/Device.h"

class MemoryAllocator {
public:
    explicit MemoryAllocator(VulkanInstance &vulkanInstance, Device &device);
    ~MemoryAllocator();

    operator VmaAllocator() const { return allocator; }

private:
    VulkanInstance &vulkanInstance;
    Device &device;

    VmaAllocator allocator;
};
