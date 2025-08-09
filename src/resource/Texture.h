#pragma once
#include "util/VulkanUtils.h"

#include "resource/ResourceManager.h"
#include "resource/Image.h"
#include "resource/Sampler.h"
#include "resource/StagingBufferManager.h"

struct Texture {
    int width, height, channels;
    std::vector<uint8_t> pixels;
    std::shared_ptr<Image> image;
    std::shared_ptr<Sampler> sampler;

    static Texture loadImage(const std::string &filePath);
    void pushTexture(ResourceManager &resourceManager, StagingBufferManager &stagingBufferManager);
};
