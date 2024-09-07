#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"

class RenderPass {
public:
    explicit RenderPass(Device &device);
    ~RenderPass();

    void create(const std::vector<VkAttachmentDescription> &attachments, const std::vector<VkSubpassDescription> &subpasses, const std::vector<VkSubpassDependency> &dependencies);

    VkRenderPass getRenderPass() const { return renderPass; }

    void begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const VkRect2D &renderArea, const std::vector<VkClearValue> &clearValues);
    void end(VkCommandBuffer commandBuffer);

    static RenderPass createDefaultRenderPass(Device &device, VkFormat colorFormat, VkFormat depthFormat);
    static RenderPass createOffscreenRenderPass(Device &device, VkFormat colorFormat);

private:
    Device &device;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};
