#pragma once
#include "util/VulkanUtils.h"

class Queue {
public:
    Queue() = delete;
    Queue(const VkQueue queue, const uint32_t familyIndex) : queue(queue), familyIndex(familyIndex) {}
    ~Queue() = default;

    operator VkQueue() const { return queue; }

    void submit(const std::vector<VkCommandBuffer> &commandBuffers, const std::vector<VkSemaphore> &waitSemaphores, const std::vector<VkPipelineStageFlags> &waitStages, const std::vector<VkSemaphore> &signalSemaphores, VkFence fence);
    void waitIdle();

    const uint32_t familyIndex;

private:
    const VkQueue queue;
};