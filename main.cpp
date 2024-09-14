#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "Engine.h"

int main(int argc, char* argv[]) {
    std::cout << "Haii wurld!! :3" << std::endl;
    srand(time(NULL));

    int width = 1600, height = 900;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    //glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
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

    //more depth bullshit
    VkImageCreateInfo depthImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };

    auto depthImage = resourceManager.createImage(depthImageCreateInfo, {.usage = VMA_MEMORY_USAGE_GPU_ONLY, .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT});

    VkImageViewCreateInfo depthImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = depthImage->getImage(),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0,1,0,1}
    };

    depthImage->createImageView(depthImageViewCreateInfo);

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

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPass.create({colorAttachment, depthAttachment}, {subpassDescription}, {subpassDependency});

    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(swapchain.getImageCount());
    for (int i = 0; i < swapchain.getImageCount(); ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i], depthImage->getImageView()}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

    auto vertCode = readFile("../shaders/vertex.spv");
    auto fragCode = readFile("../shaders/fragment.spv");
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
    auto descriptorSetData = vertexShader.getDescriptorSetLayouts();
    descriptorSetData.insert(descriptorSetData.end(), fragmentShader.getDescriptorSetLayouts().begin(), fragmentShader.getDescriptorSetLayouts().end());
    auto pushConstantRanges = vertexShader.getPushConstantRanges();
    pushConstantRanges.insert(pushConstantRanges.end(), fragmentShader.getPushConstantRanges().begin(), fragmentShader.getPushConstantRanges().end());
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(descriptorSetData, pushConstantRanges);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain.getExtent();

    ShaderModule vertModule(device, vertCode), fragModule(device, fragCode);
    VkPipeline pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setVertexInputState({{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}}, {
                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
                    {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}
            })
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setMultisampleState(VK_SAMPLE_COUNT_1_BIT)
            .setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS)
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

    CommandBuffer transferCommandBuffer = commandPool.allocateCommandBuffer();
    Mesh testingMesh = Mesh::loadObj("../dragon.obj");
    auto vertexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = testingMesh.vertices.size()*sizeof(testingMesh.vertices[0]), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    auto indexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = testingMesh.indices.size()*sizeof(testingMesh.indices[0]), .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});

    auto stagingBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 134217728, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO, .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT});
    void *mappedStagingBuffer = stagingBuffer->map();
    memcpy(mappedStagingBuffer, testingMesh.vertices.data(), testingMesh.vertices.size()*sizeof(testingMesh.vertices[0]));
    stagingBuffer->flush();

    transferCommandBuffer.begin();
    VkBufferCopy bufferCopy{0,0,testingMesh.vertices.size()*sizeof(testingMesh.vertices[0])};
    vkCmdCopyBuffer(transferCommandBuffer, stagingBuffer->getBuffer(), vertexBuffer->getBuffer(), 1, &bufferCopy);
    transferCommandBuffer.end();
    transferCommandBuffer.submit(device.getGraphicsQueue());

    vkQueueWaitIdle(device.getGraphicsQueue());

    memcpy(mappedStagingBuffer, testingMesh.indices.data(), testingMesh.indices.size()*sizeof(testingMesh.indices[0]));
    stagingBuffer->flush();

    transferCommandBuffer.begin();
    bufferCopy = {0,0,testingMesh.indices.size()*sizeof(testingMesh.indices[0])};
    vkCmdCopyBuffer(transferCommandBuffer, stagingBuffer->getBuffer(), indexBuffer->getBuffer(), 1, &bufferCopy);
    transferCommandBuffer.end();
    transferCommandBuffer.submit(device.getGraphicsQueue());

    vkQueueWaitIdle(device.getGraphicsQueue());

    stagingBuffer->unmap();

    auto atomicCounterBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 8, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT}, {.usage = VMA_MEMORY_USAGE_GPU_ONLY});
    VkDescriptorBufferInfo atomicCounterDescriptorBufferInfo{atomicCounterBuffer->getBuffer(), 0, VK_WHOLE_SIZE};
    VkDescriptorSet atomicCounterDescriptorSet;
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator).bind_buffer(0, &atomicCounterDescriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT).build(atomicCounterDescriptorSet);
    VkDescriptorSet VatomicCounterDescriptorSet;
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator).bind_buffer(0, &atomicCounterDescriptorBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build(VatomicCounterDescriptorSet);


    //Main rendering loop
    uint32_t frame = 0;
    const auto start = std::chrono::high_resolution_clock::now(); //Timestamp for rendering start

    float deltaTime = 0.0f;	// Time between current frame and last frame
    float currentFrame = 0.0f; // Time of current frame
    float lastFrame = 0.0f; // Time of last frame

    //Camera
    float sensitivity = 0.1f;
    float lastX = 0.0f, lastY = 0.0f;
    float speed = 0.5f;
    float yaw = 0.0f, pitch = 0.0f;
    glm::vec3 camera = {0.0f, 0.0f, -1.0f};
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE) continue; //Pause rendering if minimized
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break; //Exit button

        // Delta time
        currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        CommandBuffer &commandBuffer = commandBuffers[frame];

        frameLockFence[frame].wait(); //Wait for this frame to be unlocked (CPU)
        frameLockFence[frame].reset(); //Re-lock this frame (CPU)

        uint32_t swapchainIndex = swapchain.acquireNextImage(swapchainLockSemaphore[frame], VK_NULL_HANDLE);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain.getExtent().width);
        viewport.height = static_cast<float>(swapchain.getExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = swapchain.getExtent();

        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        renderPass.begin(commandBuffer, framebuffers[frame], scissor, {{}, {.depthStencil = {1.0f, 0}}});

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        //vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        //vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        //camera bs
        float time = std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1,1>>>(std::chrono::high_resolution_clock::now()-start).count();

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float xoffset = (xpos - lastX)*sensitivity;
        float yoffset = (lastY - ypos)*sensitivity;
        lastX = xpos;
        lastY = ypos;

        yaw   += xoffset;
        pitch += yoffset;

        if(pitch > 89.0f) pitch = 89.0f;
        if(pitch < -89.0f) pitch = -89.0f;

        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera += speed*deltaTime*glm::normalize(direction);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera -= speed*deltaTime*glm::normalize(direction);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera -= speed*deltaTime*glm::normalize(glm::cross(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f)));
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera += speed*deltaTime*glm::normalize(glm::cross(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f)));
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.y += speed*deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.y -= speed*deltaTime;

        glm::mat4 model = glm::scale(glm::vec3(1,1,1))*glm::rotate(glm::radians(time*2.5f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::translate(glm::vec3(0,0,0));
        glm::mat4 view = glm::lookAt(camera, camera+glm::normalize(direction), glm::vec3(0,1,0));
        glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float)width/(float)height, 0.01f, 100.0f);
        projection[1][1] *= -1;

        glm::mat4 renderMatrix = projection*view*model;

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(renderMatrix), &renderMatrix);

        double pushtime = static_cast<double>(time);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(double), &pushtime);

        commandBuffer.bindVertexBuffers(0, {vertexBuffer->getBuffer()}, {0});
        commandBuffer.bindIndexBuffer(indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        commandBuffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, {VatomicCounterDescriptorSet, atomicCounterDescriptorSet});
        commandBuffer.drawIndexed(testingMesh.indices.size());

        renderPass.end(commandBuffer);
        commandBuffer.end();

        commandBuffer.submit(device.getGraphicsQueue(), {swapchainLockSemaphore[frame]}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {renderLockSemaphore[frame]}, frameLockFence[frame]);
        if (swapchain.present(swapchainIndex, renderLockSemaphore[frame]) == 1) {
            device.waitIdle();
            glfwGetFramebufferSize(window, &width, &height);
            swapchain.create(width, height);
            framebuffers.clear();
            framebuffers.reserve(swapchain.getImageCount());

            //depth bullshit
            depthImageCreateInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
            depthImage = resourceManager.createImage(depthImageCreateInfo, {.usage = VMA_MEMORY_USAGE_GPU_ONLY, .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT});
            depthImageViewCreateInfo.image = depthImage->getImage();
            depthImage->createImageView(depthImageViewCreateInfo);

            for (int i = 0; i < swapchain.getImageCount(); ++i) {
                framebuffers.emplace_back(device, renderPass);
                framebuffers.back().create({swapchain.getImageViews()[i], depthImage->getImageView()}, swapchain.getExtent().width, swapchain.getExtent().height);
            }
        }

        frame = (frame + 1) % swapchain.getImageCount(); //Jump to next frame
    }

    device.waitIdle();

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwTerminate();
    return 0;
}