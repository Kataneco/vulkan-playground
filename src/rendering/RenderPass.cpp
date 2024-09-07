#include "RenderPass.h"

RenderPass::RenderPass(VkDevice device) : device(device) {}

RenderPass::~RenderPass() {
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }
}

RenderPass::RenderPass(RenderPass &&other) noexcept : device(other.device), renderPass(other.renderPass) {
    other.renderPass = VK_NULL_HANDLE;
}

RenderPass &RenderPass::operator=(RenderPass &&other) noexcept {
    if (this != &other) {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
        }
        device = other.device;
        renderPass = other.renderPass;
        other.renderPass = VK_NULL_HANDLE;
    }
    return *this;
}

void RenderPass::create(const std::vector<VkAttachmentDescription> &attachments, const std::vector<VkSubpassDescription> &subpasses, const std::vector<VkSubpassDependency> &dependencies) {
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = attachments.size();
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = subpasses.size();
    renderPassCreateInfo.pSubpasses = subpasses.data();
    renderPassCreateInfo.dependencyCount = dependencies.size();
    renderPassCreateInfo.pDependencies = dependencies.data();

    vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
}

void RenderPass::begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const VkRect2D &renderArea, const std::vector<VkClearValue> &clearValues) {
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderArea = renderArea;
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::end(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}