#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"
#include "CommandBuffer.h"

class CommandPool {
public:
    CommandPool(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    ~CommandPool();

    CommandPool(const CommandPool &) = delete;
    CommandPool &operator=(const CommandPool &) = delete;
    CommandPool(CommandPool &&other) noexcept;
    CommandPool &operator=(CommandPool &&other) noexcept;

    operator VkCommandPool() const { return commandPool; }

    CommandBuffer allocateCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    std::vector<CommandBuffer> allocateCommandBuffers(uint32_t count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    void reset(VkCommandPoolResetFlags flags = 0);

private:
    VkDevice device;
    VkCommandPool commandPool = VK_NULL_HANDLE;
};
