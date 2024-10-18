#pragma once
#include "util/VulkanUtils.h"

class DescriptorAllocator {
public:
    explicit DescriptorAllocator(VkDevice device);
    ~DescriptorAllocator();
    void destroy();

    struct PoolSizes {
        std::vector<std::pair<VkDescriptorType, float>> sizes =
                {
                        {VK_DESCRIPTOR_TYPE_SAMPLER,                0.5f},
                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
                        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          4.f},
                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1.f},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1.f},
                        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1.f},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2.f},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2.f},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
                        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       0.5f}
                };
    };

    void resetPools();
    bool allocate(VkDescriptorSet *set, VkDescriptorSetLayout layout);

private:
    VkDevice device;

    VkDescriptorPool grabPool();

    VkDescriptorPool currentPool = VK_NULL_HANDLE;
    PoolSizes descriptorSizes;
    std::vector<VkDescriptorPool> usedPools;
    std::vector<VkDescriptorPool> freePools;

    friend class DescriptorBuilder;
};

class DescriptorLayoutCache {
public:
    explicit DescriptorLayoutCache(VkDevice device);
    ~DescriptorLayoutCache();
    void destroy();

    VkDescriptorSetLayout createDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info);

private:
    VkDevice device;
    std::unordered_map<DescriptorSetLayoutData, VkDescriptorSetLayout> layoutCache;
};

class DescriptorBuilder {
public:
    explicit DescriptorBuilder(DescriptorLayoutCache &cache, DescriptorAllocator &alloc);

    DescriptorBuilder &bind_buffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
    DescriptorBuilder &bind_image(uint32_t binding, VkDescriptorImageInfo *imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

    bool build(VkDescriptorSet &set, VkDescriptorSetLayout &layout);
    bool build(VkDescriptorSet &set);

private:
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    DescriptorLayoutCache &cache;
    DescriptorAllocator &alloc;
};
