#pragma once
#include "util/VulkanUtils.h"

#include "core/VulkanInstance.h"

class Device {
public:
    Device(VulkanInstance &vulkanInstance, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> deviceExtensions);
    ~Device();

    operator VkDevice() const { return device; }
    operator VkPhysicalDevice() const { return physicalDevice; }

    uint32_t getGraphicsFamily() const { return graphicsFamily; }
    uint32_t getComputeFamily() const { return computeFamily; }
    uint32_t getTransferFamily() const { return transferFamily; }

    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getComputeQueue() const { return computeQueue; }
    VkQueue getTransferQueue() const { return transferQueue; }

    void waitIdle();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    VulkanInstance &vulkanInstance;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    uint32_t graphicsFamily, computeFamily, transferFamily;
    VkQueue graphicsQueue, computeQueue, transferQueue;

    friend class MemoryAllocator;
    friend class Swapchain;
};
