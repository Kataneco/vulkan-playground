#include "CommandBuffer.h"

CommandBuffer::CommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level) : device(device), commandPool(commandPool) {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
}

CommandBuffer::CommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer) : device(device), commandPool(commandPool), commandBuffer(commandBuffer) {}

CommandBuffer::~CommandBuffer() {
    if (commandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer &&other) noexcept : device(other.device), commandPool(other.commandPool), commandBuffer(other.commandBuffer) {
    other.commandBuffer = VK_NULL_HANDLE;
}

CommandBuffer &CommandBuffer::operator=(CommandBuffer &&other) noexcept {
    if (this != &other) {
        if (commandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        }
        device = other.device;
        commandPool = other.commandPool;
        commandBuffer = other.commandBuffer;
        other.commandBuffer = VK_NULL_HANDLE;
    }
    return *this;
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

void CommandBuffer::bindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline) {
    vkCmdBindPipeline(commandBuffer, bindPoint, pipeline);
}

void CommandBuffer::bindVertexBuffers(uint32_t firstBinding, const std::vector<VkBuffer> &buffers, const std::vector<VkDeviceSize> &offsets) {
    vkCmdBindVertexBuffers(commandBuffer, firstBinding, buffers.size(), buffers.data(), offsets.data());
}

void CommandBuffer::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

void CommandBuffer::bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, const std::vector<VkDescriptorSet> &descriptorSets, const std::vector<uint32_t> &dynamicOffsets) {
    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSets.size(), descriptorSets.data(), dynamicOffsets.size(), dynamicOffsets.data());
}

void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::submit(VkQueue queue, const std::vector<VkSemaphore> &waitSemaphores, const std::vector<VkPipelineStageFlags> &waitStages, const std::vector<VkSemaphore> &signalSemaphores, VkFence fence) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    vkQueueSubmit(queue, 1, &submitInfo, fence);
}
