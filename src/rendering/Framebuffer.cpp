#include "Framebuffer.h"

Framebuffer::Framebuffer(VkDevice device, VkRenderPass renderPass) : device(device), renderPass(renderPass) {}

Framebuffer::~Framebuffer() {
    destroy();
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

void Framebuffer::create(const std::vector<VkImageView> &attachments, uint32_t w, uint32_t h, uint32_t l) {
    width = w; height = h; layers = l;
    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = attachments.size();
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = layers;

    VkFramebufferAttachmentsCreateInfo FACI{};
    FACI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
    FACI.attachmentImageInfoCount = 0;

    if (attachments.empty()) {
        framebufferCreateInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
        framebufferCreateInfo.pNext = &FACI;
    }

    vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
}

void Framebuffer::destroy() {
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
        width = 0; height = 0; layers = 0;
    }
}
