#include "ShaderReflection.h"

bool DescriptorSetLayoutData::operator==(const DescriptorSetLayoutData &other) const {
    if (other.bindings.size() != bindings.size()) {
        return false;
    } else {
        for (int i = 0; i < bindings.size(); i++) {
            if (other.bindings[i].binding != bindings[i].binding)
                return false;
            if (other.bindings[i].descriptorType != bindings[i].descriptorType)
                return false;
            if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
                return false;
            if (other.bindings[i].stageFlags != bindings[i].stageFlags)
                return false;
        }
        return true;
    }
}

VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutData::getCreateInfo() {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    return createInfo;
}

ShaderReflection::ShaderReflection(const std::vector<uint32_t> &spirvCode) {
    SpvReflectResult result = spvReflectCreateShaderModule(spirvCode.size()*sizeof(uint32_t), spirvCode.data(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        //death
    }
    reflect();
}

ShaderReflection::~ShaderReflection() {
    spvReflectDestroyShaderModule(&module);
}

void ShaderReflection::reflect() {
    reflectDescriptorSets();
    reflectInputVariables();
    reflectOutputVariables();
    reflectPushConstantRanges();
}

void ShaderReflection::reflectDescriptorSets() {
    uint32_t count = 0;
    spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(count);
    spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    for (const auto *set: sets) {
        DescriptorSetLayoutData layout;
        layout.set = set->set;
        layout.bindings.reserve(set->binding_count);
        for (uint32_t i = 0; i < set->binding_count; i++) {
            const auto &reflectedBinding = set->bindings[i];
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = reflectedBinding->binding;
            binding.descriptorType = static_cast<VkDescriptorType>(reflectedBinding->descriptor_type);
            binding.descriptorCount = 1;
            for (uint32_t dim = 0; dim < reflectedBinding->array.dims_count; dim++) {
                binding.descriptorCount *= reflectedBinding->array.dims[dim];
            }
            binding.stageFlags = static_cast<VkShaderStageFlags>(module.shader_stage);
            layout.bindings.push_back(binding);
        }
        descriptorSetLayouts.push_back(layout);
    }
}

void ShaderReflection::reflectInputVariables()  {
    uint32_t count = 0;
    spvReflectEnumerateInputVariables(&module, &count, nullptr);
    std::vector<SpvReflectInterfaceVariable*> inputs(count);
    spvReflectEnumerateInputVariables(&module, &count, inputs.data());
    for (const auto *input: inputs) {
        InputVariable var;
        var.location = input->location;
        var.name = input->name;
        var.format = static_cast<VkFormat>(input->format);
        inputVariables.push_back(var);
    }
}

void ShaderReflection::reflectOutputVariables() {
    uint32_t count = 0;
    spvReflectEnumerateOutputVariables(&module, &count, nullptr);
    std::vector<SpvReflectInterfaceVariable*> outputs(count);
    spvReflectEnumerateOutputVariables(&module, &count, outputs.data());
    for (const auto *output: outputs) {
        OutputVariable var;
        var.location = output->location;
        var.name = output->name;
        var.format = static_cast<VkFormat>(output->format);
        outputVariables.push_back(var);
    }
}

void ShaderReflection::reflectPushConstantRanges() {
    uint32_t count = 0;
    spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    std::vector<SpvReflectBlockVariable*> blocks(count);
    spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
    for (const auto *block: blocks) {
        VkPushConstantRange range{};
        range.stageFlags = static_cast<VkShaderStageFlags>(module.shader_stage);
        range.offset = block->offset;
        range.size = block->size;
        pushConstantRanges.push_back(range);
    }
}
