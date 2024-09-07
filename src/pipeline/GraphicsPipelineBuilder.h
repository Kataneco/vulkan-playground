#pragma once
#include "util/VulkanUtils.h"

class GraphicsPipelineBuilder {
public:
    GraphicsPipelineBuilder &setShaders(VkShaderModule vertShader, VkShaderModule fragShader);
    GraphicsPipelineBuilder &setVertexInputState(const VkVertexInputBindingDescription &bindingDesc, const std::vector<VkVertexInputAttributeDescription> &attributeDescs);
    GraphicsPipelineBuilder &setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
    GraphicsPipelineBuilder &setViewportState(const VkViewport &viewport, const VkRect2D &scissor);
    GraphicsPipelineBuilder &setRasterizationState(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
    GraphicsPipelineBuilder &setMultisampleState(VkSampleCountFlagBits samples);
    GraphicsPipelineBuilder &setDepthStencilState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp);
    GraphicsPipelineBuilder &setColorBlendState(const VkPipelineColorBlendAttachmentState &colorBlendAttachment);
    GraphicsPipelineBuilder &setDynamicState(const std::vector<VkDynamicState> &dynamicStates);
    GraphicsPipelineBuilder &setLayout(VkPipelineLayout layout);
    GraphicsPipelineBuilder &setRenderPass(VkRenderPass renderPass, uint32_t subpass);

    VkPipeline build(VkDevice device);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
    VkPipelineViewportStateCreateInfo viewportState{};
    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    VkPipelineMultisampleStateCreateInfo multisampleState{};
    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;

    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    std::vector<VkDynamicState> dynamicStates;
    VkViewport viewport{};
    VkRect2D scissor{};
};
