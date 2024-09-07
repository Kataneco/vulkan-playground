#pragma once
#include "util/VulkanUtils.h"

class Semaphore {
public:
    explicit Semaphore(VkDevice device);
    ~Semaphore();

    Semaphore(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore(Semaphore &&other) noexcept;
    Semaphore &operator=(Semaphore &&other) noexcept;

    VkSemaphore getSemaphore() const { return semaphore; }

private:
    VkDevice device;
    VkSemaphore semaphore;
};
