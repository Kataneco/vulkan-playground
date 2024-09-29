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

VkPipelineLayout PipelineLayoutCache::createPipelineLayout(const std::vector<DescriptorSetLayoutData>& descriptorSetLayoutData, std::vector<VkPushConstantRange> pushConstantRanges) {
    std::unordered_map<uint32_t, DescriptorSetLayoutData> sets;
    for (const auto &d: descriptorSetLayoutData) {
        if (!sets.contains(d.set)) {
            sets[d.set] = d;
            continue;
        }
        for (const auto &b: d.bindings) {
            for (auto &db: sets[d.set].bindings) {
                if (b.binding == db.binding) {
                    db.stageFlags |= b.stageFlags;
                    break;
                }
            }
        }
    }

    size_t hash = 0x517cc1b727220a95;
    for (const auto &[key, d]: sets) {
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
    descriptorSetLayouts.reserve(sets.size());
    for (auto &[key, data]: sets) {
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

VkPipelineLayout PipelineLayoutCache::createPipelineLayout(const ShaderCombo &shaderCombo) {
    return createPipelineLayout(shaderCombo.descriptorSetLayouts, shaderCombo.pushConstantRanges);
}