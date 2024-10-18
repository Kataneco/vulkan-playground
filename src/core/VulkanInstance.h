#pragma once
#include "util/VulkanUtils.h"

class VulkanInstance {
public:
    VulkanInstance(uint32_t apiVersion, std::vector<const char *> instanceExtensions, bool debug = false);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance &) = delete;
    VulkanInstance &operator=(const VulkanInstance &) = delete;

    operator VkInstance() const { return instance; }

private:
    uint32_t apiVersion;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    friend class Device;
    friend class MemoryAllocator;
};
