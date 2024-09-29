#pragma once
#include "util/VulkanUtils.h"

struct DescriptorSetLayoutData {
    uint32_t set;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bool operator==(const DescriptorSetLayoutData &other) const;
    VkDescriptorSetLayoutCreateInfo getCreateInfo();
};

namespace std {
    template<> struct hash<DescriptorSetLayoutData> {
        size_t operator()(DescriptorSetLayoutData const& data) const {
            size_t result = hash<size_t>()(data.bindings.size());
            for (const auto &b: data.bindings) {
                size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
                result = hash_combine(result, binding_hash);
            }
            return result;
        }
    };
}

struct InputVariable {
    uint32_t location;
    std::string name;
    VkFormat format;
};

struct OutputVariable {
    uint32_t location;
    std::string name;
    VkFormat format;
};

class ShaderReflection {
public:
    explicit ShaderReflection(const std::string &spirvCode);
    ~ShaderReflection();

    const std::vector<DescriptorSetLayoutData> &getDescriptorSetLayouts() const { return descriptorSetLayouts; }
    const std::vector<InputVariable> &getInputVariables() const { return inputVariables; }
    const std::vector<OutputVariable> &getOutputVariables() const { return outputVariables; }
    const std::vector<VkPushConstantRange> &getPushConstantRanges() const { return pushConstantRanges; }

private:
    SpvReflectShaderModule module;
    std::vector<DescriptorSetLayoutData> descriptorSetLayouts;
    std::vector<InputVariable> inputVariables;
    std::vector<OutputVariable> outputVariables;
    std::vector<VkPushConstantRange> pushConstantRanges;

    void reflect();
    void reflectDescriptorSets();
    void reflectInputVariables();
    void reflectOutputVariables();
    void reflectPushConstantRanges();
};

struct ShaderCombo {
    std::vector<DescriptorSetLayoutData> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    ShaderCombo &operator+=(const ShaderReflection &o);
    ShaderCombo &operator+=(const ShaderCombo &o);

private:
    void mergeDescriptorSetLayouts(const std::vector<DescriptorSetLayoutData> &other);
    void mergePushConstantRanges(const std::vector<VkPushConstantRange> &other);
};

ShaderCombo operator+(const ShaderReflection &a, const ShaderReflection &b);