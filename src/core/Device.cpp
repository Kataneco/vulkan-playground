#include "Device.h"

Device::Device(VulkanInstance& vulkanInstance, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> deviceExtensions) : vulkanInstance(vulkanInstance) {
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
    deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);

    uint32_t ione = 1;
    vkEnumeratePhysicalDevices(vulkanInstance.instance, &ione, &physicalDevice);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    auto *queueFamilies = new VkQueueFamilyProperties[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        const VkQueueFlags flags = queueFamilies[i].queueFlags;
        if (flags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        else if (flags & VK_QUEUE_COMPUTE_BIT) computeFamily = i;
        else if (flags & VK_QUEUE_TRANSFER_BIT) transferFamily = i;
    }

    delete[] queueFamilies;

    float defaultPriorities[64];
    defaultPriorities[0] = 1.0f;

    VkDeviceQueueCreateInfo graphicsQueueCreateInfo{};
    graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = defaultPriorities;
    graphicsQueueCreateInfo.queueFamilyIndex = graphicsFamily;

    VkDeviceQueueCreateInfo computeQueueCreateInfo{};
    computeQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    computeQueueCreateInfo.queueCount = 1;
    computeQueueCreateInfo.pQueuePriorities = defaultPriorities;
    computeQueueCreateInfo.queueFamilyIndex = computeFamily;

    VkDeviceQueueCreateInfo transferQueueCreateInfo{};
    transferQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    transferQueueCreateInfo.queueCount = 1;
    transferQueueCreateInfo.pQueuePriorities = defaultPriorities;
    transferQueueCreateInfo.queueFamilyIndex = transferFamily;

    VkDeviceQueueCreateInfo queueCreateInfos[3] = {graphicsQueueCreateInfo, computeQueueCreateInfo, transferQueueCreateInfo};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 3;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

    volkLoadDevice(device);

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
    vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);
}

Device::~Device() {
    vkDestroyDevice(device, nullptr);
}

void Device::waitIdle() {
    vkDeviceWaitIdle(device);
}
