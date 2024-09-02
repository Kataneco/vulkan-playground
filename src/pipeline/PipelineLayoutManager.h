#pragma once
#include "util/VulkanUtils.h"

#include "shader/ShaderReflection.h"
#include "descriptor/DescriptorSetManager.h"

class PipelineLayoutCache {
public:
    PipelineLayoutCache(VkDevice device, DescriptorLayoutCache& descriptorLayoutCache);
    ~PipelineLayoutCache();
    void destroy();

    VkPipelineLayout createPipelineLayout(std::vector<DescriptorSetLayoutData> descriptorSetLayoutData, std::vector<VkPushConstantRange> pushConstantRanges);

private:
    VkDevice device;
    DescriptorLayoutCache& descriptorLayoutCache;
    std::unordered_map<size_t, VkPipelineLayout> layoutCache;
};