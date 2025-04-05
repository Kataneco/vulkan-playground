#pragma once
#include "util/VulkanUtils.h"

//TODO: Add pNext for other stuff or automate extension info chaining

class GraphicsPipelineBuilder {
public:
    GraphicsPipelineBuilder &setShaders(VkShaderModule vertShader, VkShaderModule fragShader);
    GraphicsPipelineBuilder &setShaders(VkShaderModule vertShader, VkShaderModule geomShader, VkShaderModule fragShader);
    GraphicsPipelineBuilder &setVertexInputState(const std::vector<VkVertexInputBindingDescription> &bindingDescs, const std::vector<VkVertexInputAttributeDescription> &attributeDescs);
    GraphicsPipelineBuilder &setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable = VK_FALSE);
    GraphicsPipelineBuilder &setViewportState(const VkViewport &viewport, const VkRect2D &scissor);
    GraphicsPipelineBuilder &setRasterizationState(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, float lineWidth = 1.0f, void* pNext = nullptr);
    GraphicsPipelineBuilder &setMultisampleState(VkSampleCountFlagBits samples);
    GraphicsPipelineBuilder &setDepthStencilState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp);
    GraphicsPipelineBuilder &setColorBlendState(const VkPipelineColorBlendAttachmentState &colorBlendAttachment);
    GraphicsPipelineBuilder &setColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState> &colorBlendAttachments);
    GraphicsPipelineBuilder &setDynamicState(const std::vector<VkDynamicState> &dynamicStates);
    GraphicsPipelineBuilder &setLayout(VkPipelineLayout layout);
    GraphicsPipelineBuilder &setRenderPass(VkRenderPass renderPass, uint32_t subpass);

    //GraphicsPipelineBuilder &chain(void* next);

    VkPipeline build(VkDevice device);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
    VkPipelineVertexInputStateCreateInfo vertexInputState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, .primitiveRestartEnable = VK_FALSE};
    VkPipelineViewportStateCreateInfo viewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rasterizationState{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo multisampleState{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE};
    VkPipelineDepthStencilStateCreateInfo depthStencilState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_FALSE};
    VkPipelineColorBlendStateCreateInfo colorBlendState{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};
    VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    std::vector<VkDynamicState> dynamicStates;
    VkViewport viewport{};
    VkRect2D scissor{};

    //std::vector<void*> links;
};
