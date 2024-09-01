#pragma once
#include "util/VulkanUtils.h"

VkResult createShaderModule(VkDevice device, size_t codeSize, const char* code, VkShaderModule* shaderModule);
