#pragma once
#include "../../Engine.h"

/*
class ImageNode {
public:
    std::shared_ptr<Image> image;
    VkFormat format;
    uint32_t width, height;
    VkImageUsageFlags usage;

    glm::vec2 position;

    void create(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, ResourceManager& resourceManager) {
        image = resourceManager.createImage({
              .imageType = VK_IMAGE_TYPE_2D,
              .format = format,
              .extent = {width, height, 1},
              .mipLevels = 1,
              .arrayLayers = 1,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .tiling = VK_IMAGE_TILING_OPTIMAL,
              .usage = usage
      });
    }
};
*/