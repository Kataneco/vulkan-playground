#include "ShaderModule.h"

VkShaderModule createShaderModule(VkDevice device, size_t codeSize, const char* code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = codeSize;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code);
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    return shaderModule;
}

VkShaderModule createShaderModule(VkDevice device, const std::string &code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    return shaderModule;
}

ShaderModule::ShaderModule(VkDevice device, size_t codeSize, const char *code) : device(device), shaderModule(VK_NULL_HANDLE) {
    shaderModule = createShaderModule(device, codeSize, code);
}

ShaderModule::ShaderModule(VkDevice device, const std::string &code) : device(device), shaderModule(VK_NULL_HANDLE) {
    shaderModule = createShaderModule(device, code);
}

ShaderModule::~ShaderModule() {
    if (shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
}

ShaderModule::ShaderModule(ShaderModule &&other) noexcept : device(other.device), shaderModule(other.shaderModule) {
    other.shaderModule = VK_NULL_HANDLE;
}

ShaderModule &ShaderModule::operator=(ShaderModule &&other) noexcept {
    if (this != &other) {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
        }
        device = other.device;
        shaderModule = other.shaderModule;
        other.shaderModule = VK_NULL_HANDLE;
    }
    return *this;
}
