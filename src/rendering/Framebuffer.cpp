#include "Framebuffer.h"

Framebuffer::Framebuffer(Device &device, RenderPass &renderPass) : device(device), renderPass(renderPass) {}

Framebuffer::~Framebuffer() {
    vkDestroyFramebuffer(device.getDevice(), framebuffer, nullptr);
}

void Framebuffer::create(const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height, uint32_t layers) {
    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass.getRenderPass();
    framebufferCreateInfo.attachmentCount = attachments.size();
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = layers;

    vkCreateFramebuffer(device.getDevice(), &framebufferCreateInfo, nullptr, &framebuffer);
}