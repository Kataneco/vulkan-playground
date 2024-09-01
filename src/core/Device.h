#pragma once
#include "util/VulkanUtils.h"

#include "core/VulkanInstance.h"

class Device {
public:
    explicit Device(VulkanInstance &vulkanInstance, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> deviceExtensions);
    ~Device();

    VkDevice getDevice() const { return device; }

    uint32_t getGraphicsFamily() const { return graphicsFamily; }
    uint32_t getComputeFamily() const { return computeFamily; }
    uint32_t getTransferFamily() const { return transferFamily; }

    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getComputeQueue() const { return computeQueue; }
    VkQueue getTransferQueue() const { return transferQueue; }

private:
    VulkanInstance &vulkanInstance;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    uint32_t graphicsFamily, computeFamily, transferFamily;
    VkQueue graphicsQueue, computeQueue, transferQueue;

    friend class MemoryAllocator;
    friend class SwapchainManager;
};
