#include "Framebuffer.h"

Framebuffer::Framebuffer(VkDevice device, VkRenderPass renderPass) : device(device), renderPass(renderPass) {}

Framebuffer::~Framebuffer() {
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
}

Framebuffer::Framebuffer(Framebuffer &&other) noexcept : device(other.device), renderPass(other.renderPass), framebuffer(other.framebuffer) {
    other.framebuffer = VK_NULL_HANDLE;
}

Framebuffer &Framebuffer::operator=(Framebuffer &&other) noexcept {
    if (this != &other) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        device = other.device;
        renderPass = other.renderPass;
        framebuffer = other.framebuffer;
        other.framebuffer = VK_NULL_HANDLE;
    }
    return *this;
}

void Framebuffer::create(const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height, uint32_t layers) {
    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = attachments.size();
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = layers;

    vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
}