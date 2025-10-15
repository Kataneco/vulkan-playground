#include "Device.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;

    bool complete() const {
        return graphics.has_value();
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR /*surface*/) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        const auto& family = queueFamilies[i];
        if (!indices.graphics && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.graphics = i;
        }
        if (!indices.compute && (family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.compute = i;
        }
        if (!indices.transfer && (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            indices.transfer = i;
        }
    }

    if (!indices.compute && indices.graphics) {
        indices.compute = indices.graphics;
    }
    if (!indices.transfer && indices.graphics) {
        indices.transfer = indices.graphics;
    }

    return indices;
}

bool supportsExtensions(VkPhysicalDevice physicalDevice, const std::vector<const char*>& extensions) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    for (const char* extension : extensions) {
        const bool found = std::any_of(availableExtensions.begin(), availableExtensions.end(),
            [extension](const VkExtensionProperties& properties) {
                return std::strcmp(properties.extensionName, extension) == 0;
            });
        if (!found) {
            return false;
        }
    }
    return true;
}

int scorePhysicalDevice(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    int score = 0;
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    score += static_cast<int>(properties.limits.maxImageDimension2D);
    return score;
}
}

Device::Device(VulkanInstance& vulkanInstance, VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> deviceExtensions)
    : vulkanInstance(vulkanInstance) {
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    deviceExtensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);

    uint32_t physicalDeviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(vulkanInstance.instance, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS || physicalDeviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable physical devices were found");
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(vulkanInstance.instance, &physicalDeviceCount, physicalDevices.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate physical devices");
    }

    int bestScore = -1;
    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    QueueFamilyIndices selectedQueues;

    for (VkPhysicalDevice candidate : physicalDevices) {
        auto queueIndices = findQueueFamilies(candidate, VK_NULL_HANDLE);
        if (!queueIndices.complete()) {
            continue;
        }
        if (!supportsExtensions(candidate, deviceExtensions)) {
            continue;
        }

        const int score = scorePhysicalDevice(candidate);
        if (score > bestScore) {
            bestScore = score;
            selectedDevice = candidate;
            selectedQueues = queueIndices;
        }
    }

    if (selectedDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to select a suitable physical device");
    }

    physicalDevice = selectedDevice;
    graphicsFamily = selectedQueues.graphics.value();
    computeFamily = selectedQueues.compute.value();
    transferFamily = selectedQueues.transfer.value();

    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, computeFamily, transferFamily};

    float defaultPriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (uint32_t familyIndex : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = familyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &defaultPriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.imagelessFramebuffer = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.maintenance4 = VK_TRUE;

    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenance1Features{};
    swapchainMaintenance1Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    swapchainMaintenance1Features.swapchainMaintenance1 = VK_TRUE;

    vulkan12Features.pNext = &vulkan13Features;
    vulkan13Features.pNext = &swapchainMaintenance1Features;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &vulkan12Features;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    volkLoadDevice(device);

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
    vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);
}

Device::~Device() {
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }
}

void Device::waitIdle() {
    vkDeviceWaitIdle(device);
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}
