#include "CommandBuffer.h"

CommandBuffer::CommandBuffer(Device &device, CommandPool &commandPool, VkCommandBufferLevel level) : device(device), commandPool(commandPool) {
    commandBuffer = commandPool.allocateCommandBuffer(level);
}

CommandBuffer::~CommandBuffer() {
    commandPool.freeCommandBuffer(commandBuffer);
}

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = flags;

    vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
}

void CommandBuffer::end() {
    vkEndCommandBuffer(commandBuffer);
}

void CommandBuffer::reset(VkCommandBufferResetFlags flags) {
    vkResetCommandBuffer(commandBuffer, flags);
}