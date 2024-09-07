#pragma once
#include "util/VulkanUtils.h"

#include "core/Device.h"
#include "RenderPass.h"

class Framebuffer {
public:
    explicit Framebuffer(Device &device, RenderPass &renderPass);
    ~Framebuffer();

    void create(const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height, uint32_t layers = 1);

    VkFramebuffer getFramebuffer() const { return framebuffer; }

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getLayers() const { return layers; }

    static Framebuffer createSwapChainFramebuffer(Device &device, RenderPass &renderPass, const std::vector<VkImageView> &attachments, uint32_t width, uint32_t height);

private:
    Device &device;
    RenderPass &renderPass;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    uint32_t width = 0, height = 0, layers = 0;
};
