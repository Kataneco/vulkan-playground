#include "CommandPool.h"

CommandPool::CommandPool(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) : device(device) {
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = flags;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
}

CommandPool::~CommandPool() {
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
}

CommandPool::CommandPool(CommandPool &&other) noexcept : device(other.device), commandPool(other.commandPool) {
    other.commandPool = VK_NULL_HANDLE;
}

CommandPool &CommandPool::operator=(CommandPool &&other) noexcept {
    if (this != &other) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
        device = other.device;
        commandPool = other.commandPool;
        other.commandPool = VK_NULL_HANDLE;
    }
    return *this;
}

CommandBuffer CommandPool::allocateCommandBuffer(VkCommandBufferLevel level) {
    return {device, commandPool, level};
}

std::vector<CommandBuffer> CommandPool::allocateCommandBuffers(uint32_t count, VkCommandBufferLevel level) {
    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.resize(count);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = count;
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data());

    std::vector<CommandBuffer> superCommandBuffers;
    superCommandBuffers.reserve(count);
    for (const auto& commandBuffer : commandBuffers) {
        superCommandBuffers.emplace_back(device, commandPool, commandBuffer);
    }

    return superCommandBuffers;
}

void CommandPool::reset(VkCommandPoolResetFlags flags) {
    vkResetCommandPool(device, commandPool, flags);
}