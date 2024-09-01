#pragma once
#include "util/VulkanUtils.h"

class VulkanInstance {
public:
    explicit VulkanInstance(uint32_t apiVersion, std::vector<const char *> instanceExtensions);
    ~VulkanInstance();

private:
    uint32_t apiVersion;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    friend class Device;
    friend class MemoryAllocator;
};
