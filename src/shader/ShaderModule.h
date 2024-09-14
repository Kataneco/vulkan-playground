#pragma once
#include "util/VulkanUtils.h"

VkShaderModule createShaderModule(VkDevice device, size_t codeSize, const char* code);
VkShaderModule createShaderModule(VkDevice device, const std::string &code);

class ShaderModule {
public:
    ShaderModule(VkDevice device, size_t codeSize, const char* code);
    ShaderModule(VkDevice device, const std::string &code);
    ~ShaderModule();

    ShaderModule(const ShaderModule &) = delete;
    ShaderModule &operator=(const ShaderModule &) = delete;
    ShaderModule(ShaderModule &&other) noexcept;
    ShaderModule &operator=(ShaderModule &&other) noexcept;

    operator VkShaderModule() { return shaderModule; }
    operator VkShaderModule*() { return &shaderModule; }

private:
    VkDevice device;
    VkShaderModule shaderModule;
};