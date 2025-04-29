#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"
#include "util/Window.h"

class Swapchain {
public:
    Swapchain(Device &device, Window &window);
    ~Swapchain();

    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    size_t getImageCount() const { return imageCount; }
    const std::vector<VkImageView> &getImageViews() const { return imageViews; }

    uint32_t acquireNextImage(VkSemaphore semaphore, VkFence fence);
    uint32_t present(uint32_t imageIndex, VkSemaphore waitSemaphore);

private:
    void create();
    void destroy();

    Device &device;
    Window &window;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    uint32_t imageCount = 0;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat imageFormat;
    VkExtent2D extent;
};