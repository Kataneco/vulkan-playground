#include "ShaderModule.h"

VkResult createShaderModule(VkDevice device, size_t codeSize, const char* code, VkShaderModule* shaderModule) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = codeSize;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code);

    return vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, shaderModule);
}
