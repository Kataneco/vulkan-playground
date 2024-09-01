#pragma once
#include "util/VulkanUtils.h"

#include "Resource.h"
#include "MemoryAllocator.h"

class Image : public Resource {
public:
    Image(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo &imageInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &name);
    ~Image() override;

    void destroy() override;

    VkImage getImage() const { return image; }
    VmaAllocation getAllocation() const { return allocation; }
    VkImageView getImageView() const { return imageView; }

    void createImageView(const VkImageViewCreateInfo &viewInfo);

private:
    VmaAllocator allocator;
    VkImage image;
    VmaAllocation allocation;
    VkImageView imageView;
};