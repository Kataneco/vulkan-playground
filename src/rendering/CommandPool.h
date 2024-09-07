#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"

class CommandPool {
public:
    explicit CommandPool(Device &device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    ~CommandPool();

    VkCommandPool getCommandPool() const { return commandPool; }

    VkCommandBuffer allocateCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    void freeCommandBuffer(VkCommandBuffer commandBuffer);
    void freeCommandBuffers(const std::vector<VkCommandBuffer> &commandBuffers);
    void reset(VkCommandPoolResetFlags flags = 0);

private:
    Device &device;
    VkCommandPool commandPool = VK_NULL_HANDLE;
};
