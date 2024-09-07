#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"

class SwapchainManager {
public:
    explicit SwapchainManager(Device &device, VkSurfaceKHR surface);
    ~SwapchainManager();

    void create(int width, int height);
    void destroy();

    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    size_t getImageCount() const { return imageCount; }
    const std::vector<VkImageView> &getImageViews() const { return imageViews; }

    uint32_t acquireNextImage(VkSemaphore semaphore, VkFence fence);
    void present(uint32_t imageIndex, VkSemaphore waitSemaphore);

private:
    Device &device;
    VkSurfaceKHR surface;

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