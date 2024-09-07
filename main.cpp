#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include "Engine.h"

int main(int argc, char* argv[]) {
    std::cout << "Haii wurld!! :3" << std::endl;

    int width = 1280, height = 720;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    //glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(width, height, "I <3 You", nullptr, nullptr);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    VulkanInstance instance(VK_API_VERSION_1_3, instanceExtensions);

    Device device(instance, {.samplerAnisotropy = VK_TRUE}, {});

    DescriptorLayoutCache descriptorLayoutCache(device);
    DescriptorAllocator descriptorAllocator(device);
    MemoryAllocator memoryAllocator(instance, device);
    ResourceManager resourceManager(device, memoryAllocator);
    CommandPool commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    PipelineLayoutCache pipelineLayoutCache(device, descriptorLayoutCache);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(instance, window, nullptr, &surface);

    Swapchain swapchain(device, surface);
    swapchain.create(width, height);

    RenderPass renderPass(device);
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    //subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPass.create({colorAttachment}, {subpassDescription}, {subpassDependency});

    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(swapchain.getImageCount());
    for (int i = 0; i < swapchain.getImageCount(); ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i]}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

    auto vertCode = readFile("../shaders/vertex.spv");
    auto fragCode = readFile("../shaders/fragment.spv");
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
    //auto descriptorSetData = vertexShader.getDescriptorSetLayouts();
    //descriptorSetData.insert(descriptorSetData.end(), fragmentShader.getDescriptorSetLayouts().begin(), fragmentShader.getDescriptorSetLayouts().end());
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout({}, {});

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain.getExtent();

    VkShaderModule vertModule = createShaderModule(device, vertCode), fragModule = createShaderModule(device, fragCode);
    VkPipeline pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setMultisampleState(VK_SAMPLE_COUNT_1_BIT)
            //.setColorBlendState(colorBlendAttachment)
            //.setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
            .setLayout(pipelineLayout)
            .setRenderPass(renderPass, 0)
            .build(device);

    std::vector<Semaphore> swapchainLockSemaphore;
    std::vector<Semaphore> renderLockSemaphore;
    std::vector<Fence> frameLockFence;

    for (uint32_t i = 0; i < swapchain.getImageCount(); ++i) {
        swapchainLockSemaphore.emplace_back(device);
        renderLockSemaphore.emplace_back(device);
        frameLockFence.emplace_back(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    std::vector<CommandBuffer> commandBuffers = commandPool.allocateCommandBuffers(swapchain.getImageCount());

    uint32_t frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE) continue; //Pause rendering if minimized
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break; //Exit button

        CommandBuffer &commandBuffer = commandBuffers[frame];

        frameLockFence[frame].wait(); //Wait for this frame to be unlocked (CPU)
        frameLockFence[frame].reset(); //Re-lock this frame (CPU)

        uint32_t swapchainIndex = swapchain.acquireNextImage(swapchainLockSemaphore[frame], VK_NULL_HANDLE);

        /*
        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain.getExtent().width);
        viewport.height = static_cast<float>(swapchain.getExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = swapchain.getExtent();
        */

        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        renderPass.begin(commandBuffer, framebuffers[frame], scissor, {{}});

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        //vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        //vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        commandBuffer.draw(3);

        renderPass.end(commandBuffer);
        commandBuffer.end();

        commandBuffer.submit(device.getGraphicsQueue(), {swapchainLockSemaphore[frame]}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {renderLockSemaphore[frame]}, frameLockFence[frame]);
        if (swapchain.present(swapchainIndex, renderLockSemaphore[frame]) == 1) {
            device.waitIdle();
            glfwGetFramebufferSize(window, &width, &height);
            swapchain.create(width, height);
            framebuffers.clear();
            framebuffers.reserve(swapchain.getImageCount());
            for (int i = 0; i < swapchain.getImageCount(); ++i) {
                framebuffers.emplace_back(device, renderPass);
                framebuffers.back().create({swapchain.getImageViews()[i]}, swapchain.getExtent().width, swapchain.getExtent().height);
            }
        }

        frame = (frame + 1) % swapchain.getImageCount(); //Jump to next frame
    }

    device.waitIdle();

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwTerminate();
    return 0;
}