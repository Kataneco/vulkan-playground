#pragma once
#include "util/VulkanUtils.h"

VkShaderModule createShaderModule(VkDevice device, size_t codeSize, const char* code);

VkShaderModule createShaderModule(VkDevice device, std::string code);
