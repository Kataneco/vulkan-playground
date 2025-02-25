#define KITTY_MAIN
#include "Engine.h"
#include <random>

int main(int argc, char* argv[]) {
    std::cout << "Haii wurld!! :3" << std::endl;
    srand(time(NULL));

    int width = 1600, height = 900;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    Window window(width, height);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    VulkanInstance instance(VK_API_VERSION_1_3, instanceExtensions, true);
    Device device(instance, {.independentBlend = VK_TRUE, .geometryShader = VK_TRUE, .fillModeNonSolid = VK_TRUE, .samplerAnisotropy = VK_TRUE, .vertexPipelineStoresAndAtomics = VK_TRUE, .fragmentStoresAndAtomics = VK_TRUE}, {});

    DescriptorLayoutCache descriptorLayoutCache(device);
    DescriptorAllocator descriptorAllocator(device);
    MemoryAllocator memoryAllocator(instance, device);
    ResourceManager resourceManager(device, memoryAllocator);
    StagingBufferManager stagingBufferManager(device, 64 * 1024 * 1024);
    CommandPool commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    PipelineLayoutCache pipelineLayoutCache(device, descriptorLayoutCache);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(instance, window, nullptr, &surface);

    Swapchain swapchain(device, surface);
    swapchain.create(width, height);

    VkImageCreateInfo depthImageCreateInfo{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };

    auto depthImage = resourceManager.createImage(depthImageCreateInfo, {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    VkImageViewCreateInfo depthImageViewCreateInfo{
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
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthAttachmentReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPass.create({colorAttachment, depthAttachment}, {subpass}, {subpassDependency});

    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(swapchain.getImageCount());
    for (int i = 0; i < swapchain.getImageCount(); ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i], depthImage->getImageView()}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

    auto vertCode = readFile("shaders/voxeldisplay.vert.spv");
    auto fragCode = readFile("shaders/voxeldisplay.frag.spv");
    ShaderModule vertModule(device, vertCode), fragModule(device, fragCode);
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(vertexShader+fragmentShader);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain.getExtent();

    VkPipeline pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setColorBlendState({noBlend})
            .setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS)
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

    //Experimental
    Mesh dragon = Mesh::loadObj("/home/honeywrap/Documents/kitten/assets/dragon.obj");
    dragon.pushMesh(resourceManager, stagingBufferManager);

    Mesh bunny = Mesh::loadObj("/home/honeywrap/Documents/kitten/assets/bunny.obj");
    bunny.pushMesh(resourceManager, stagingBufferManager);

    auto objectData = resourceManager.createBuffer({.size = sizeof(glm::mat4x4)*1024, .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});

    auto voxelBuffer = resourceManager.createBuffer({.size = sizeof(Voxel)*1024*1024*2, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
    //auto svoBuffer = resourceManager.createBuffer({.size = sizeof(OctreeNode)*1024*1024*4, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
    auto voxelCountBuffer = resourceManager.createBuffer({.size = sizeof(uint64_t), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT}); //Atomix
    //auto nodeCountBuffer = resourceManager.createBuffer({.size = sizeof(uint64_t), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}); //Atomix

    auto voxelDrawIndirectBuffer = resourceManager.createBuffer({.size = sizeof(VkDrawIndirectCommand), .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});
    VkDrawIndirectCommand drawIndirectCommand{
        36,
        0,
        0,
        0
    };

    uint64_t zero = 0;
    stagingBufferManager.stageBufferData(&zero, voxelCountBuffer->getBuffer(), sizeof(zero));
    //stagingBufferManager.stageBufferData(&zero, nodeCountBuffer->getBuffer(), sizeof(zero));
    stagingBufferManager.stageBufferData(&drawIndirectCommand, voxelDrawIndirectBuffer->getBuffer(), sizeof(drawIndirectCommand));
    stagingBufferManager.flush();

    VkDescriptorBufferInfo voxelDataSetInfos[] = {
            {voxelCountBuffer->getBuffer(), 0, sizeof(uint32_t)},
            {voxelBuffer->getBuffer(), 0, VK_WHOLE_SIZE}
    };

    VkDescriptorSet voxelizerDataSet;
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
            .bind_buffer(0, &voxelDataSetInfos[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(1, &voxelDataSetInfos[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(voxelizerDataSet);

    //Object Specific
    VkDescriptorSet dragonSet;
    VkDescriptorBufferInfo dragonSetInfo = {objectData->getBuffer(), 0, sizeof(glm::mat4x4)};
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
            .bind_buffer(0, &dragonSetInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(dragonSet);

    VkDescriptorSet bunnySet;
    VkDescriptorBufferInfo bunnySetInfo = {objectData->getBuffer(), sizeof(glm::mat4x4), sizeof(glm::mat4x4)};
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
            .bind_buffer(0, &bunnySetInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(bunnySet);

    //Voxeldisplay exclusive
    VkDescriptorSet voxeldisplayDataSet;
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
            .bind_buffer(0, &voxelDataSetInfos[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(voxeldisplayDataSet);

    //Experimental
    auto voxelVertCode = readFile("shaders/voxelize.vert.spv");
    auto voxelGeomCode = readFile("shaders/voxelize.geom.spv");
    auto voxelFragCode = readFile("shaders/voxelize.frag.spv");
    ShaderModule voxelVertModule(device, voxelVertCode), voxelGeomModule(device, voxelGeomCode), voxelFragModule(device, voxelFragCode);
    ShaderReflection voxelVertexShader(voxelVertCode), voxelGeometryShader(voxelGeomCode), voxelFragmentShader(voxelFragCode);
    VkPipelineLayout voxelizerPipelineLayout = pipelineLayoutCache.createPipelineLayout((voxelVertexShader+voxelGeometryShader)+voxelFragmentShader);

    RenderPass voxelizerPass(device);
    VkSubpassDescription voxelizerSubpass{};
    voxelizerSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkSubpassDependency voxelizerDependency{};
    voxelizerDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    voxelizerDependency.dstSubpass = 0;
    voxelizerDependency.srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    voxelizerDependency.srcAccessMask = 0;
    voxelizerDependency.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    voxelizerDependency.dstAccessMask = 0;

    voxelizerPass.create({}, {voxelizerSubpass}, {});

    VoxelizerData dragonVoxelizerConstants{
            {0, 0, 0},
            {2048, 2048, 1.0f/512.0f}
    };

    glm::vec4 grid = glm::vec4(dragonVoxelizerConstants.center, dragonVoxelizerConstants.resolution.z);

    VkViewport voxelizerViewport{};
    voxelizerViewport.width = dragonVoxelizerConstants.resolution.x;
    voxelizerViewport.height = dragonVoxelizerConstants.resolution.y;
    voxelizerViewport.minDepth = 0.0f;
    voxelizerViewport.maxDepth = 1.0f;

    VkRect2D voxelizerScissor{};
    voxelizerScissor.extent = {static_cast<uint32_t>(dragonVoxelizerConstants.resolution.x), static_cast<uint32_t>(dragonVoxelizerConstants.resolution.y)};

    VkPipeline voxelizerPipeline = GraphicsPipelineBuilder()
            .setShaders(voxelVertModule, voxelGeomModule, voxelFragModule)
            .setVertexInputState(Vertex::bindings(), Vertex::attributes())
            .setViewportState(voxelizerViewport, voxelizerScissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setLayout(voxelizerPipelineLayout)
            .setRenderPass(voxelizerPass, 0)
            .build(device);

    Framebuffer imagelessFramebuffer(device, voxelizerPass);
    imagelessFramebuffer.create({}, dragonVoxelizerConstants.resolution.x, dragonVoxelizerConstants.resolution.y);

    //Main rendering loop
    uint32_t frame = 0;
    const auto start = std::chrono::high_resolution_clock::now(); //Timestamp for rendering start

    float deltaTime = 0.0f;    // Time between current frame and last frame
    float currentFrame = 0.0f; // Time of current frame
    float lastFrame = 0.0f; // Time of last frame

    //Camera
    float sensitivity = 0.1f;
    float lastX = 0.0f, lastY = 0.0f;
    float speed = 0.75f;
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

        //Camera bullshit
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

        glm::mat4 view = glm::lookAt(camera, camera+glm::normalize(direction), glm::vec3(0,1,0));
        glm::mat4 projection = glm::perspective(glm::radians(75.0f), (float)width/(float)height, 0.0001f, 5000.0f);
        projection[1][1] *= -1;
        glm::mat4 unified = projection*view;

        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        //Experimental
        glm::mat4 model = glm::scale(glm::vec3(1.0f,1.0f,1.0f)*0.3f)*glm::rotate(glm::radians(time*2.5f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::translate(glm::vec3(0,0,0));
        glm::mat4 modelb = glm::scale(glm::vec3(1.0f,1.0f,1.0f)*0.3f)*glm::rotate(glm::radians(time*-1.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::translate(glm::vec3(1,0,0));
        stagingBufferManager.stageBufferData(&model, objectData->getBuffer(), sizeof(glm::mat4x4));
        stagingBufferManager.stageBufferData(&modelb, objectData->getBuffer(), sizeof(glm::mat4x4), sizeof(glm::mat4x4));
        stagingBufferManager.flush();

        voxelizerPass.begin(commandBuffer, imagelessFramebuffer, voxelizerScissor, {});

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, voxelizerPipeline);
        commandBuffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, voxelizerPipelineLayout, 0, {dragonSet, voxelizerDataSet});

        vkCmdPushConstants(commandBuffer, voxelizerPipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(dragonVoxelizerConstants), &dragonVoxelizerConstants);

        commandBuffer.bindVertexBuffers(0, {dragon.vertexBuffer->getBuffer()}, {0});
        commandBuffer.bindIndexBuffer(dragon.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        commandBuffer.drawIndexed(dragon.indices.size());

        commandBuffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, voxelizerPipelineLayout, 0, {bunnySet, voxelizerDataSet});
        commandBuffer.bindVertexBuffers(0, {bunny.vertexBuffer->getBuffer()}, {0});
        commandBuffer.bindIndexBuffer(bunny.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        commandBuffer.drawIndexed(bunny.indices.size());

        voxelizerPass.end(commandBuffer);

        ResourceBarrier::bufferMemoryBarrier(commandBuffer, voxelCountBuffer->getBuffer(), sizeof(uint32_t), sizeof(uint32_t), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferCopy vcUpdate{};
        vcUpdate.size = sizeof(uint32_t);
        vcUpdate.srcOffset = 0;
        vcUpdate.dstOffset = sizeof(uint32_t);
        vkCmdCopyBuffer(commandBuffer, voxelCountBuffer->getBuffer(), voxelDrawIndirectBuffer->getBuffer(), 1, &vcUpdate);

        ResourceBarrier::bufferMemoryBarrier(commandBuffer, voxelDrawIndirectBuffer->getBuffer(), sizeof(uint32_t), sizeof(uint32_t), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

        renderPass.begin(commandBuffer, framebuffers[frame], scissor, {{.color = {0.0f, 0.0f, 0.0f, 0.0f}}, {.depthStencil = {1.0f, 0}}});

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        commandBuffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, {voxeldisplayDataSet});

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(unified), &unified);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(unified), sizeof(grid), &grid);

        vkCmdDrawIndirect(commandBuffer, voxelDrawIndirectBuffer->getBuffer(), 0, 1, sizeof(VkDrawIndirectCommand));

        renderPass.end(commandBuffer);

        ResourceBarrier::bufferMemoryBarrier(commandBuffer, voxelCountBuffer->getBuffer(), 0, sizeof(uint64_t), VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT);

        VkBufferCopy vcReset{};
        vcReset.size = sizeof(uint32_t);
        vcReset.srcOffset = sizeof(uint32_t);
        vcReset.dstOffset = 0;
        vkCmdCopyBuffer(commandBuffer, voxelCountBuffer->getBuffer(), voxelCountBuffer->getBuffer(), 1, &vcReset);

        commandBuffer.end();

        commandBuffer.submit(device.getGraphicsQueue(), {swapchainLockSemaphore[frame]}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {renderLockSemaphore[frame]}, frameLockFence[frame]);

        if (swapchain.present(swapchainIndex, renderLockSemaphore[frame]) == 1) {
            device.waitIdle();
            //glfwGetFramebufferSize(window, &width, &height);
               //death
        }

        frame = (frame + 1) % swapchain.getImageCount(); //Jump to next frame
    }

    device.waitIdle();

    vkDestroyPipeline(device, voxelizerPipeline, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    swapchain.destroy();
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwTerminate();
    return 0;
}