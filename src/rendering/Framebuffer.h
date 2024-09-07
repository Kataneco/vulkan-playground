#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"
#include "RenderPass.h"

class Framebuffer {
public:
    Framebuffer(VkDevice device, VkRenderPass renderPass);
    ~Framebuffer();

    Framebuffer(const Framebuffer &) = delete;
    Framebuffer &operator=(const Framebuffer &) = delete;
    Framebuffer(Framebuffer &&other) noexcept;
    Framebuffer &operator=(Framebuffer &&other) noexcept;

    void create(const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height, uint32_t layers = 1);

    operator VkFramebuffer() const { return framebuffer; }

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getLayers() const { return layers; }

    static Framebuffer createSwapChainFramebuffer(VkDevice device, VkRenderPass renderPass, const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height);

private:
    VkDevice device;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    uint32_t width = 0, height = 0, layers = 0;
};
