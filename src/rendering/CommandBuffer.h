#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"
#include "CommandPool.h"

class CommandBuffer {
public:
    CommandBuffer(Device &device, CommandPool &commandPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~CommandBuffer();

    VkCommandBuffer getCommandBuffer() const { return commandBuffer; }

    void begin(VkCommandBufferUsageFlags flags = 0);
    void end();
    void reset(VkCommandBufferResetFlags flags = 0);
    void submit(VkQueue queue, const std::vector<VkSemaphore> &waitSemaphores = {}, const std::vector<VkPipelineStageFlags> &waitStages = {}, const std::vector<VkSemaphore> &signalSemaphores = {}, VkFence fence = VK_NULL_HANDLE);

    void bindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline);
    void bindVertexBuffers(uint32_t firstBinding, const std::vector<VkBuffer> &buffers, const std::vector<VkDeviceSize> &offsets);
    void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
    void bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, const std::vector<VkDescriptorSet> &descriptorSets, const std::vector<uint32_t> &dynamicOffsets = {});
    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);
    void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

private:
    Device &device;
    CommandPool &commandPool;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};
