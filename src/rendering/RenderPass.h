#pragma once
#include "util/VulkanUtils.h"

class RenderPass {
public:
    explicit RenderPass(VkDevice device);
    ~RenderPass();

    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    RenderPass(RenderPass &&other) noexcept;
    RenderPass &operator=(RenderPass &&other) noexcept;

    void create(const std::vector<VkAttachmentDescription> &attachments, const std::vector<VkSubpassDescription> &subpasses, const std::vector<VkSubpassDependency> &dependencies);

    operator VkRenderPass() const { return renderPass; }

    void begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const VkRect2D &renderArea, const std::vector<VkClearValue> &clearValues);
    void end(VkCommandBuffer commandBuffer);

    static RenderPass createDefaultRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    static RenderPass createOffscreenRenderPass(VkDevice device, VkFormat colorFormat);

private:
    VkDevice device;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};
