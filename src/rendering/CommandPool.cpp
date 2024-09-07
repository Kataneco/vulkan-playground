#include "CommandPool.h"

CommandPool::CommandPool(Device &device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) : device(device) {
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = flags;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    vkCreateCommandPool(device.getDevice(), &commandPoolCreateInfo, nullptr, &commandPool);
}

CommandPool::~CommandPool() {
    vkDestroyCommandPool(device.getDevice(), commandPool, nullptr);
}

VkCommandBuffer CommandPool::allocateCommandBuffer(VkCommandBufferLevel level) {
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device.getDevice(), &commandBufferAllocateInfo, &commandBuffer);
    return commandBuffer;
}

std::vector<VkCommandBuffer> CommandPool::allocateCommandBuffers(uint32_t count, VkCommandBufferLevel level) {
    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.resize(count);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = count;

    vkAllocateCommandBuffers(device.getDevice(), &commandBufferAllocateInfo, commandBuffers.data());
    return commandBuffers;
}

void CommandPool::freeCommandBuffer(VkCommandBuffer commandBuffer) {
    vkFreeCommandBuffers(device.getDevice(), commandPool, 1, &commandBuffer);
}

void CommandPool::freeCommandBuffers(const std::vector<VkCommandBuffer> &commandBuffers) {
    vkFreeCommandBuffers(device.getDevice(), commandPool, commandBuffers.size(), commandBuffers.data());
}

void CommandPool::reset(VkCommandPoolResetFlags flags) {
    vkResetCommandPool(device.getDevice(), commandPool, flags);
}