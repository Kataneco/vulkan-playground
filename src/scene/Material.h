#pragma once
#include "util/VulkanUtils.h"

struct Material {
    Material(VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet = VK_NULL_HANDLE);

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};