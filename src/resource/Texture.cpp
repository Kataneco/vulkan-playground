#include "Texture.h"

Texture Texture::loadImage(const std::string &filePath) {
    Texture texture;
    stbi_uc *pixels = stbi_load(filePath.c_str(), &texture.width, &texture.height, &texture.channels, 4);
    //TODO wtf
    texture.channels = 4;

    if (!pixels) {
        //TODO: make default texture
        throw std::runtime_error("Missing image");
    }

    texture.pixels.resize(texture.width * texture.height * texture.channels);
    memcpy(texture.pixels.data(), pixels, texture.pixels.size());

    stbi_image_free(pixels);
    return texture;
}

VkFormat getFormat(int channels) {
    switch (channels) {
        case 1:
            return VK_FORMAT_R8_UNORM;
        case 2:
            return VK_FORMAT_R8G8_UNORM;
        case 3:
            return VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return VK_FORMAT_R8G8B8A8_SRGB;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

void Texture::pushTexture(ResourceManager &resourceManager, StagingBufferManager &stagingBufferManager) {
    image = resourceManager.createImage({.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = getFormat(channels), .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    image->createImageView({.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = image->getImage(), .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = getFormat(channels), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}});
    sampler = resourceManager.createSampler({.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .anisotropyEnable = VK_TRUE, .maxAnisotropy = 16});
    stagingBufferManager.stageImageData(pixels.data(), image->getImage(), pixels.size(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1});
    stagingBufferManager.flush();
}