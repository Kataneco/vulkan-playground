#pragma once
#include "util/VulkanUtils.h"

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