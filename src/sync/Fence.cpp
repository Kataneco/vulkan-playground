#include "Fence.h"

Fence::Fence(VkDevice device, VkFenceCreateFlags flags) : device(device), fence(VK_NULL_HANDLE) {
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = flags;
    vkCreateFence(device, &createInfo, nullptr, &fence);
}

Fence::~Fence() {
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(device, fence, nullptr);
    }
}

Fence::Fence(Fence&& other) noexcept : device(other.device), fence(other.fence) {
    other.fence = VK_NULL_HANDLE;
}

Fence& Fence::operator=(Fence&& other) noexcept {
    if (this != &other) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device, fence, nullptr);
        }
        device = other.device;
        fence = other.fence;
        other.fence = VK_NULL_HANDLE;
    }
    return *this;
}

void Fence::wait(uint64_t timeout) {
    vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
}

void Fence::reset() {
    vkResetFences(device, 1, &fence);
}