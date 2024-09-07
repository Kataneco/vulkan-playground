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

VkShaderModule createShaderModule(VkDevice device, std::string code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    return shaderModule;
}
