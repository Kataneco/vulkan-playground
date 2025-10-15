#include "Renderer.h"

#include <filesystem>
#include <stdexcept>

#include "resource/Texture.h"

namespace {
std::vector<const char*> gatherInstanceExtensions() {
    std::vector<const char*> extensions = Window::getRequiredInstanceExtensions();
    return extensions;
}
}

Renderer::Renderer(const RendererCreateInfo& createInfo)
    : config(createInfo)
    , instance(VK_API_VERSION_1_3, gatherInstanceExtensions(), config.enableValidation)
    , device(instance, config.enabledFeatures, config.additionalDeviceExtensions)
    , window(config.width, config.height, config.title.c_str())
    , swapchain(device, window)
    , renderPass(device)
    , commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) {
    if (!config.iconPath.empty()) {
        try {
            Texture icon = Texture::loadImage(config.iconPath);
            window.setWindowIcon(icon, icon);
        } catch (const std::exception&) {
            // Ignore icon loading failures to keep the editor portable
        }
    }

    createSwapchainResources();
}

Renderer::~Renderer() {
    device.waitIdle();
    destroySwapchainResources();
}

std::optional<Renderer::Frame> Renderer::beginFrame() {
    Fence& fence = inFlightFences[currentFrame];
    fence.wait();
    fence.reset();

    uint32_t imageIndex = swapchain.acquireNextImage(imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE);
    if (imageIndex == UINT32_MAX) {
        recreateSwapchain();
        return std::nullopt;
    }

    CommandBuffer& commandBuffer = commandBuffers[currentFrame];
    commandBuffer.reset();
    commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    Framebuffer& framebuffer = framebuffers[imageIndex];
    return Frame{commandBuffer, framebuffer, imageIndex, currentFrame, swapchain.getExtent()};
}

void Renderer::beginSwapchainRenderPass(const Frame& frame, const std::vector<VkClearValue>& clearValues) {
    VkRect2D renderArea{};
    renderArea.extent = frame.extent;
    renderPass.begin(frame.commandBuffer, frame.framebuffer, renderArea, clearValues);
}

void Renderer::endSwapchainRenderPass(const Frame& frame) {
    renderPass.end(frame.commandBuffer);
}

void Renderer::submitFrame(const Frame& frame) {
    frame.commandBuffer.end();

    const std::vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[frame.frameIndex]};
    const std::vector<VkPipelineStageFlags> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const std::vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[frame.frameIndex]};

    frame.commandBuffer.submit(device.getGraphicsQueue(), waitSemaphores, waitStages, signalSemaphores, inFlightFences[frame.frameIndex]);

    const uint32_t presentResult = swapchain.present(frame.imageIndex, renderFinishedSemaphores[frame.frameIndex]);
    if (presentResult == 1) {
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % static_cast<uint32_t>(commandBuffers.size());
}

void Renderer::recreateSwapchain() {
    device.waitIdle();
    destroySwapchainResources();
    swapchain.recreate();
    createSwapchainResources();
}

void Renderer::createSwapchainResources() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    renderPass.create({colorAttachment}, {subpass}, {dependency});

    const size_t imageCount = swapchain.getImageCount();
    framebuffers.clear();
    framebuffers.reserve(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i]}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

    commandBuffers = commandPool.allocateCommandBuffers(static_cast<uint32_t>(imageCount));

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
    imageAvailableSemaphores.reserve(imageCount);
    renderFinishedSemaphores.reserve(imageCount);
    inFlightFences.reserve(imageCount);

    for (size_t i = 0; i < imageCount; ++i) {
        imageAvailableSemaphores.emplace_back(device);
        renderFinishedSemaphores.emplace_back(device);
        inFlightFences.emplace_back(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    currentFrame = 0;
}

void Renderer::destroySwapchainResources() {
    framebuffers.clear();
    commandBuffers.clear();
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
}
