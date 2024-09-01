#include "SwapchainManager.h"

SwapchainManager::SwapchainManager(Device &device, VkSurfaceKHR surface) : device(device), surface(surface) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &surfaceCapabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, nullptr);
    surfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, surfaceFormats.data());
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, nullptr);
    presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, presentModes.data());

    //XXX
    imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
}

SwapchainManager::~SwapchainManager() {
    destroy();
}

void SwapchainManager::create(int width, int height) {
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (imageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device, imageViews[i], nullptr);
            imageViews[i] = VK_NULL_HANDLE;
        }
    }

    extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = 3;
    swapchainCreateInfo.imageFormat = imageFormat;
    swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = swapchain;

    vkCreateSwapchainKHR(device.device, &swapchainCreateInfo, nullptr, &swapchain);

    vkGetSwapchainImagesKHR(device.device, swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device.device, swapchain, &imageCount, images.data());
    imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = images[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = imageFormat;
        imageViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(device.device, &imageViewCreateInfo, nullptr, &imageViews[i]);
    }
}

void SwapchainManager::destroy() {
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (imageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device, imageViews[i], nullptr);
            imageViews[i] = VK_NULL_HANDLE;
        }
    }
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device.device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

uint32_t SwapchainManager::acquireNextImage(VkSemaphore semaphore, VkFence fence) {
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device.device, swapchain, UINT64_MAX, semaphore, fence, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        create(extent.width, extent.height);
        return UINT32_MAX;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        //death

    }

    return imageIndex;
}

void SwapchainManager::present(uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;

    VkResult result = vkQueuePresentKHR(device.graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        create(extent.width, extent.height);
    } else if (result != VK_SUCCESS) {
        //death

    }
}