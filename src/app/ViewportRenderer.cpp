#include "ViewportRenderer.h"

#include <array>
#include <stdexcept>
#include <vector>

#include <imgui_impl_vulkan.h>

#include "app/EditorUI.h"
#include "util/VulkanUtils.h"

namespace {
constexpr VkPipelineColorBlendAttachmentState kAlphaBlend{
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};
}

ViewportRenderer::ViewportRenderer(Device& device,
                                   ResourceManager& resourceManager,
                                   PipelineLayoutCache& pipelineLayoutCache,
                                   EditorUI& editorUI,
                                   uint32_t width,
                                   uint32_t height)
    : device(device)
    , resourceManager(resourceManager)
    , pipelineLayoutCache(pipelineLayoutCache)
    , editorUI(editorUI)
    , renderPass(device)
    , framebuffer(device, renderPass) {
    textureExtent = {width, height};

    targetImage = resourceManager.createImage({
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .extent = {textureExtent.width, textureExtent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    targetImage->createImageView({
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = targetImage->getFormat(),
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    });

    sampler = resourceManager.createSampler({});
    descriptorSet = editorUI.registerTexture(sampler->getSampler(), targetImage->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkAttachmentDescription attachment{};
    attachment.format = targetImage->getFormat();
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference attachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    renderPass.create({attachment}, {subpass}, {dependency});
    framebuffer.create({targetImage->getImageView()}, textureExtent.width, textureExtent.height);

    const std::string vertCode = readFile("shaders/fullscreenQuad.vert.spv");
    const std::string fragCode = readFile("shaders/meow.frag.spv");

    ShaderModule vertModule(device, vertCode);
    ShaderModule fragModule(device, fragCode);
    ShaderReflection vertexShader(vertCode);
    ShaderReflection fragmentShader(fragCode);

    ShaderCombo shaderCombo = vertexShader + fragmentShader;
    pipelineLayout = pipelineLayoutCache.createPipelineLayout(shaderCombo);

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(textureExtent.width);
    viewport.height = static_cast<float>(textureExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = textureExtent;

    pipeline = GraphicsPipelineBuilder()
        .setShaders(vertModule, fragModule)
        .setViewportState(viewport, scissor)
        .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 1.0f)
        .setColorBlendState({kAlphaBlend})
        .setLayout(pipelineLayout)
        .setRenderPass(renderPass, 0)
        .setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
        .build(device);
}

ViewportRenderer::~ViewportRenderer() {
    if (descriptorSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(descriptorSet);
    }
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
}

void ViewportRenderer::record(CommandBuffer& commandBuffer, VkExtent2D /*swapchainExtent*/, float elapsedSeconds, float deltaTime, const ImVec2& viewportSize) {
    const VkRect2D renderArea{{0, 0}, textureExtent};
    const std::array<VkClearValue, 1> clearValues{{VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}}}};
    const std::vector<VkClearValue> clearVector(clearValues.begin(), clearValues.end());

    renderPass.begin(commandBuffer, framebuffer, renderArea, clearVector);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    glm::vec4 data{viewportSize.x, viewportSize.y, elapsedSeconds, deltaTime};
    commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    commandBuffer.pushConstants(pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
    commandBuffer.draw(3);

    renderPass.end(commandBuffer);
}
