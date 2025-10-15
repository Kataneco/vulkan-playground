#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/Device.h"
#include "core/Swapchain.h"
#include "core/VulkanInstance.h"
#include "rendering/CommandBuffer.h"
#include "rendering/CommandPool.h"
#include "rendering/Framebuffer.h"
#include "rendering/RenderPass.h"
#include "sync/Fence.h"
#include "sync/Semaphore.h"
#include "util/Window.h"

struct RendererCreateInfo {
    uint32_t width = 1600;
    uint32_t height = 900;
    std::string title = "Kitten Editor";
    bool enableValidation = true;
    VkPhysicalDeviceFeatures enabledFeatures{};
    std::vector<const char*> additionalDeviceExtensions{};
    std::string iconPath;
};

class Renderer {
public:
    struct Frame {
        CommandBuffer& commandBuffer;
        Framebuffer& framebuffer;
        uint32_t imageIndex;
        uint32_t frameIndex;
        VkExtent2D extent;
    };

    explicit Renderer(const RendererCreateInfo& createInfo);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    VulkanInstance& getInstance() { return instance; }
    Device& getDevice() { return device; }
    Window& getWindow() { return window; }
    Swapchain& getSwapchain() { return swapchain; }
    RenderPass& getRenderPass() { return renderPass; }
    CommandPool& getCommandPool() { return commandPool; }

    std::optional<Frame> beginFrame();
    void beginSwapchainRenderPass(const Frame& frame, const std::vector<VkClearValue>& clearValues);
    void endSwapchainRenderPass(const Frame& frame);
    void submitFrame(const Frame& frame);
    void recreateSwapchain();

private:
    void createSwapchainResources();
    void destroySwapchainResources();

    RendererCreateInfo config;
    VulkanInstance instance;
    Device device;
    Window window;
    Swapchain swapchain;
    RenderPass renderPass;
    CommandPool commandPool;
    std::vector<CommandBuffer> commandBuffers;
    std::vector<Framebuffer> framebuffers;
    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<Semaphore> renderFinishedSemaphores;
    std::vector<Fence> inFlightFences;
    uint32_t currentFrame = 0;
};
