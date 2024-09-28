#include "Queue.h"

void Queue::submit(const std::vector<VkCommandBuffer>& commandBuffers, const std::vector<VkSemaphore>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages, const std::vector<VkSemaphore>& signalSemaphores, VkFence fence) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    vkQueueSubmit(queue, 1, &submitInfo, fence);
}

void Queue::waitIdle() {
    vkQueueWaitIdle(queue);
}