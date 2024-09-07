#include "ResourceManager.h"

ResourceManager::ResourceManager(Device& device, MemoryAllocator& memoryAllocator) : device(device), memoryAllocator(memoryAllocator) {}

ResourceManager::~ResourceManager() {
    cleanup();
}

std::shared_ptr<Buffer> ResourceManager::createBuffer(const VkBufferCreateInfo& bufferInfo, const VmaAllocationCreateInfo& allocInfo, const std::string& debugName) {
    auto buffer = std::make_shared<Buffer>(device, memoryAllocator.getAllocator(), bufferInfo, allocInfo, debugName);
    if (!debugName.empty()) {
        setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t) buffer->getBuffer(), debugName);
        buffers[debugName] = buffer;
    }
    return buffer;
}

std::shared_ptr<Image> ResourceManager::createImage(const VkImageCreateInfo& imageInfo, const VmaAllocationCreateInfo& allocInfo, const std::string& debugName) {
    auto image = std::make_shared<Image>(device, memoryAllocator.getAllocator(), imageInfo, allocInfo, debugName);
    if (!debugName.empty()) {
        setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t) image->getImage(), debugName);
        images[debugName] = image;
    }
    return image;
}

std::shared_ptr<Sampler> ResourceManager::createSampler(const VkSamplerCreateInfo& samplerInfo, const std::string& debugName) {
    auto sampler = std::make_shared<Sampler>(device, samplerInfo, debugName);
    if (!debugName.empty()) {
        setDebugName(VK_OBJECT_TYPE_SAMPLER, (uint64_t) sampler->getSampler(), debugName);
        samplers[debugName] = sampler;
    }
    return sampler;
}

std::shared_ptr<Buffer> ResourceManager::getBuffer(const std::string& debugName) const {
    auto it = buffers.find(debugName);
    return (it != buffers.end()) ? it->second : nullptr;
}

std::shared_ptr<Image> ResourceManager::getImage(const std::string& debugName) const {
    auto it = images.find(debugName);
    return (it != images.end()) ? it->second : nullptr;
}

std::shared_ptr<Sampler> ResourceManager::getSampler(const std::string& debugName) const {
    auto it = samplers.find(debugName);
    return (it != samplers.end()) ? it->second : nullptr;
}

void ResourceManager::destroyBuffer(const std::string& debugName) {
    auto it = buffers.find(debugName);
    if (it != buffers.end()) {
        buffers.erase(it);
    }
}

void ResourceManager::destroyImage(const std::string& debugName) {
    auto it = images.find(debugName);
    if (it != images.end()) {
        images.erase(it);
    }
}

void ResourceManager::destroySampler(const std::string& debugName) {
    auto it = samplers.find(debugName);
    if (it != samplers.end()) {
        samplers.erase(it);
    }
}

void ResourceManager::cleanup() {
    buffers.clear();
    images.clear();
    samplers.clear();
}

void ResourceManager::setDebugName(VkObjectType objectType, uint64_t object, const std::string& name) {
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = object;
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}
