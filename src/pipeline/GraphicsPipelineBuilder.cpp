#include "GraphicsPipelineBuilder.h"

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setShaders(VkShaderModule vertShader, VkShaderModule fragShader) {
    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertShader;
    vertexShaderStageCreateInfo.pName = "main";

    shaderStages.push_back(vertexShaderStageCreateInfo);

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragShader;
    fragmentShaderStageCreateInfo.pName = "main";

    shaderStages.push_back(fragmentShaderStageCreateInfo);
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setVertexInputState(const std::vector<VkVertexInputBindingDescription>& bindingDescs, const std::vector<VkVertexInputAttributeDescription>& attributeDescs) {
    bindingDescriptions = bindingDescs;
    attributeDescriptions = attributeDescs;

    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = bindingDescriptions.size();
    vertexInputState.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputState.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setInputAssemblyState(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable) {
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = topology;
    inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setViewportState(const VkViewport& vp, const VkRect2D& sc) {
    viewport = vp;
    scissor = sc;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setRasterizationState(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace) {
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = polygonMode;
    rasterizationState.cullMode = cullMode;
    rasterizationState.frontFace = frontFace;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setMultisampleState(VkSampleCountFlagBits samples) {
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = samples;
    multisampleState.sampleShadingEnable = VK_FALSE;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthStencilState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp) {
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = depthTestEnable;
    depthStencilState.depthWriteEnable = depthWriteEnable;
    depthStencilState.depthCompareOp = depthCompareOp;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setColorBlendState(const VkPipelineColorBlendAttachmentState& attachment) {
    colorBlendAttachment = attachment;

    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;
    return *this;
}

GraphicsPipelineBuilder &GraphicsPipelineBuilder::setColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState> &attachments) {
    colorBlendAttachments = attachments;

    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = colorBlendAttachments.size();
    colorBlendState.pAttachments = colorBlendAttachments.data();
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDynamicState(const std::vector<VkDynamicState>& states) {
    dynamicStates = states;

    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setLayout(VkPipelineLayout layout) {
    pipelineLayout = layout;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setRenderPass(VkRenderPass pass, uint32_t subpassIndex) {
    renderPass = pass;
    subpass = subpassIndex;
    return *this;
}

VkPipeline GraphicsPipelineBuilder::build(VkDevice device) {
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState = &multisampleState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = dynamicStates.empty() ? nullptr : &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpass;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        //death
    }

    return pipeline;
}
