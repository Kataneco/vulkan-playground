#include "Image.h"

Image::Image(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo &imageInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &name) : Resource(device, name), allocator(allocator) {
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
}

Image::~Image() {
    destroy();
}

void Image::destroy() {
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

void Image::createImageView(const VkImageViewCreateInfo& viewInfo) {
    vkCreateImageView(device, &viewInfo, nullptr, &imageView);
}
