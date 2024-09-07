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
    VkPipelineVertexInputStateCreateInfo vertexInputState{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rasterizationState{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo multisampleState{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo depthStencilState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendStateCreateInfo colorBlendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;

    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    std::vector<VkDynamicState> dynamicStates;
    VkViewport viewport{};
    VkRect2D scissor{};
};
