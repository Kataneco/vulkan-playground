#pragma once
#include "node.h"

class FramebufferNode : public GraphNode {
private:
    VkDevice device;
    ResourceManager* resourceManager;
    VkDescriptorPool descriptorPool;

public:
    VkFormat format;
    VkExtent3D extent;

    std::shared_ptr<Image> targetImage;
    std::shared_ptr<Sampler> sampler;
    VkDescriptorSet displaySet;

    // FIXME: WHAT THE FUCK?????????????????????
    // FIXME: THIS IS FUCKING CURSED!!!
    std::unique_ptr<RenderPass>* renderPassReference;
    std::unique_ptr<Framebuffer> framebuffer;

    FramebufferNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device, std::unique_ptr<RenderPass>* renderPass);
    ~FramebufferNode() override;

    void CreateRenderTarget();

    const char* GetTypeName() const override { return "Render Target"; }

    void Draw() override;
    void DrawProperties() override;
};