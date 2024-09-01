#pragma once
#include "util/VulkanUtils.h"

#include "Buffer.h"
#include "Image.h"
#include "Sampler.h"

class ResourceManager {
public:
    ResourceManager(Device &device, MemoryAllocator &memoryAllocator);
    ~ResourceManager();

    std::shared_ptr<Buffer> createBuffer(const VkBufferCreateInfo &bufferInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &debugName = "");
    std::shared_ptr<Image> createImage(const VkImageCreateInfo &imageInfo, const VmaAllocationCreateInfo &allocInfo, const std::string &debugName = "");
    std::shared_ptr<Sampler> createSampler(const VkSamplerCreateInfo &samplerInfo, const std::string &debugName = "");

    std::shared_ptr<Buffer> getBuffer(const std::string &debugName) const;
    std::shared_ptr<Image> getImage(const std::string &debugName) const;
    std::shared_ptr<Sampler> getSampler(const std::string &debugName) const;

    void destroyBuffer(const std::string &debugName);
    void destroyImage(const std::string &debugName);
    void destroySampler(const std::string &debugName);

    void cleanup();

private:
    Device &device;
    MemoryAllocator &memoryAllocator;

    std::unordered_map<std::string, std::shared_ptr<Buffer>> buffers;
    std::unordered_map<std::string, std::shared_ptr<Image>> images;
    std::unordered_map<std::string, std::shared_ptr<Sampler>> samplers;

    void setDebugName(VkObjectType objectType, uint64_t object, const std::string &name);
};
