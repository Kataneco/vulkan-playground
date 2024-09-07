#pragma once
#include "util/VulkanUtils.h"

class Fence {
public:
    explicit Fence(VkDevice device, VkFenceCreateFlags flags = 0);
    ~Fence();

    Fence(const Fence &) = delete;
    Fence &operator=(const Fence &) = delete;
    Fence(Fence &&other) noexcept;
    Fence &operator=(Fence &&other) noexcept;

    operator VkFence() { return fence; }
    operator VkFence*() { return &fence; }

    VkFence getFence() const { return fence; }

    void wait(uint64_t timeout = UINT64_MAX);
    void reset();

private:
    VkDevice device;
    VkFence fence;
};
