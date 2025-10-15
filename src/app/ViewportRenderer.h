#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "app/EditorUI.h"
#include "app/Renderer.h"
#include "pipeline/GraphicsPipelineBuilder.h"
#include "pipeline/PipelineLayoutManager.h"
#include "rendering/RenderPass.h"
#include "resource/Image.h"
#include "resource/ResourceManager.h"
#include "resource/Sampler.h"
#include "resource/StagingBufferManager.h"
#include "shader/ShaderModule.h"
#include "shader/ShaderReflection.h"

class ViewportRenderer {
public:
    ViewportRenderer(Device& device,
                     ResourceManager& resourceManager,
                     PipelineLayoutCache& pipelineLayoutCache,
                     EditorUI& editorUI,
                     uint32_t width,
                     uint32_t height);
    ~ViewportRenderer();

    ViewportRenderer(const ViewportRenderer&) = delete;
    ViewportRenderer& operator=(const ViewportRenderer&) = delete;

    void record(CommandBuffer& commandBuffer, VkExtent2D swapchainExtent, float elapsedSeconds, float deltaTime, const ImVec2& viewportSize);

    VkDescriptorSet getViewportDescriptor() const { return descriptorSet; }
    VkExtent2D getTextureExtent() const { return textureExtent; }

private:
    Device& device;
    ResourceManager& resourceManager;
    PipelineLayoutCache& pipelineLayoutCache;
    EditorUI& editorUI;

    std::shared_ptr<Image> targetImage;
    std::shared_ptr<Sampler> sampler;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    RenderPass renderPass;
    Framebuffer framebuffer;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkViewport viewport{};
    VkRect2D scissor{};
    VkExtent2D textureExtent{};
};
