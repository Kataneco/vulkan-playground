#pragma once
#include "util/VulkanUtils.h"

#include "shader/ShaderReflection.h"
#include "descriptor/DescriptorSetManager.h"

class PipelineLayoutCache {
public:
    explicit PipelineLayoutCache(VkDevice device, DescriptorLayoutCache& descriptorLayoutCache);
    ~PipelineLayoutCache();
    void destroy();

    VkPipelineLayout createPipelineLayout(const std::vector<DescriptorSetLayoutData>& descriptorSetLayoutData, std::vector<VkPushConstantRange> pushConstantRanges);
    VkPipelineLayout createPipelineLayout(const ShaderCombo& shaderCombo);

private:
    VkDevice device;
    DescriptorLayoutCache& descriptorLayoutCache;
    std::unordered_map<size_t, VkPipelineLayout> layoutCache;
};