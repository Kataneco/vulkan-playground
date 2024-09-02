#include "PipelineLayoutManager.h"

PipelineLayoutCache::PipelineLayoutCache(VkDevice device, DescriptorLayoutCache &descriptorLayoutCache) : device(device), descriptorLayoutCache(descriptorLayoutCache) {}

PipelineLayoutCache::~PipelineLayoutCache() {
    destroy();
}

void PipelineLayoutCache::destroy() {
    for (auto pair: layoutCache) {
        vkDestroyPipelineLayout(device, pair.second, nullptr);
    }
}

VkPipelineLayout PipelineLayoutCache::createPipelineLayout(std::vector<DescriptorSetLayoutData> descriptorSetLayoutData, std::vector<VkPushConstantRange> pushConstantRanges) {
    size_t hash = 0x517cc1b727220a95;
    for (const auto &d: descriptorSetLayoutData) {
        hash = hash_combine(hash, std::hash<DescriptorSetLayoutData>()(d));
    }
    for (const auto &p: pushConstantRanges) {
        size_t packed = p.offset | p.size << 16;
        packed |= ((size_t) p.stageFlags) << 32;
        hash = hash_combine(hash, packed);
    }

    auto it = layoutCache.find(hash);
    if (it != layoutCache.end())
        return (*it).second;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.reserve(descriptorSetLayoutData.size());
    for (auto &data: descriptorSetLayoutData) {
        auto dsci = data.getCreateInfo();
        descriptorSetLayouts.push_back(descriptorLayoutCache.createDescriptorLayout(&dsci));
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    layoutCache[hash] = pipelineLayout;
    return pipelineLayout;
}