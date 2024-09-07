#include "Semaphore.h"

Semaphore::Semaphore(VkDevice device) : device(device), semaphore(VK_NULL_HANDLE) {
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device, &createInfo, nullptr, &semaphore);
}

Semaphore::~Semaphore() {
    if (semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
}

Semaphore::Semaphore(Semaphore&& other) noexcept : device(other.device), semaphore(other.semaphore) {
    other.semaphore = VK_NULL_HANDLE;
}

Semaphore& Semaphore::operator=(Semaphore&& other) noexcept {
    if (this != &other) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        device = other.device;
        semaphore = other.semaphore;
        other.semaphore = VK_NULL_HANDLE;
    }
    return *this;
}
