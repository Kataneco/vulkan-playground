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

ShaderReflection::ShaderReflection(const std::string &spirvCode) {
    SpvReflectResult result = spvReflectCreateShaderModule(spirvCode.size(), spirvCode.data(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        //death
    }
    reflect();
    //spvReflectDestroyShaderModule(&module);
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

void ShaderCombo::mergeDescriptorSetLayouts(const std::vector<DescriptorSetLayoutData>& other) {
    std::unordered_map<uint32_t, DescriptorSetLayoutData> setMap;

    for (const auto& layout : descriptorSetLayouts) {
        setMap[layout.set] = std::move(layout);
    }

    for (const auto& newLayout : other) {
        auto& existingLayout = setMap[newLayout.set];
        for (const auto& newBinding : newLayout.bindings) {
            auto it = std::find_if(existingLayout.bindings.begin(), existingLayout.bindings.end(),
                                   [&](const auto& b) { return b.binding == newBinding.binding; });

            if (it == existingLayout.bindings.end()) {
                existingLayout.bindings.push_back(newBinding);
            } else {
                it->stageFlags |= newBinding.stageFlags;
            }
        }
    }

    descriptorSetLayouts.clear();
    for (auto& [_, layout] : setMap) {
        descriptorSetLayouts.push_back(std::move(layout));
    }
}

void ShaderCombo::mergePushConstantRanges(const std::vector<VkPushConstantRange>& other) {
    std::unordered_map<uint64_t, VkPushConstantRange> rangeMap;

    auto addToMap = [&rangeMap](const VkPushConstantRange &range) {
        uint64_t key = (static_cast<uint64_t>(range.offset) << 32) | range.size;
        auto &existing = rangeMap[key];
        if (existing.size == 0) {
            existing = range;
        } else {
            existing.stageFlags |= range.stageFlags;
        }
    };

    for (const auto &range: pushConstantRanges) {
        addToMap(range);
    }
    for (const auto &range: other) {
        addToMap(range);
    }

    pushConstantRanges.clear();
    for (const auto &[_, range]: rangeMap) {
        pushConstantRanges.push_back(range);
    }
}

ShaderCombo& ShaderCombo::operator+=(const ShaderReflection& o) {
    mergeDescriptorSetLayouts(o.getDescriptorSetLayouts());
    mergePushConstantRanges(o.getPushConstantRanges());
    return *this;
}

ShaderCombo& ShaderCombo::operator+=(const ShaderCombo& o) {
    mergeDescriptorSetLayouts(o.descriptorSetLayouts);
    mergePushConstantRanges(o.pushConstantRanges);
    return *this;
}

ShaderCombo operator+(const ShaderReflection &a, const ShaderReflection &b) {
    ShaderCombo result;
    result += a;
    result += b;
    return result;
}