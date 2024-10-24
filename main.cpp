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
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    VulkanInstance instance(VK_API_VERSION_1_3, instanceExtensions, true);

    Device device(instance, {.independentBlend = VK_TRUE, .fillModeNonSolid = VK_TRUE, .samplerAnisotropy = VK_TRUE}, {});

    DescriptorLayoutCache descriptorLayoutCache(device);
    DescriptorAllocator descriptorAllocator(device);
    MemoryAllocator memoryAllocator(instance, device);
    ResourceManager resourceManager(device, memoryAllocator);
    StagingBufferManager stagingBufferManager(device, 64*1024*1024);
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

    auto gColor = resourceManager.createImage({
                                                      .imageType = VK_IMAGE_TYPE_2D,
                                                      .format = VK_FORMAT_R8G8B8A8_UNORM,
                                                      .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                      .mipLevels = 1,
                                                      .arrayLayers = 1,
                                                      .samples = VK_SAMPLE_COUNT_1_BIT,
                                                      .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                                              }, {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    auto gPosition = resourceManager.createImage({
                                                         .imageType = VK_IMAGE_TYPE_2D,
                                                         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                                         .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                         .mipLevels = 1,
                                                         .arrayLayers = 1,
                                                         .samples = VK_SAMPLE_COUNT_1_BIT,
                                                         .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                         .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                                                 }, {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    auto gNormalSpec = resourceManager.createImage({
                                                         .imageType = VK_IMAGE_TYPE_2D,
                                                         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                                         .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                         .mipLevels = 1,
                                                         .arrayLayers = 1,
                                                         .samples = VK_SAMPLE_COUNT_1_BIT,
                                                         .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                         .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                                                   }, {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_GPU_ONLY});

    gColor->createImageView({
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                            });

    gPosition->createImageView({
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                       .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                               });

    gNormalSpec->createImageView({
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                       .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                               });

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

    VkAttachmentDescription gColorAttachment{};
    gColorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    gColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gPositionAttachment{};
    gPositionAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    gPositionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gPositionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gPositionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gPositionAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gNormalSpecAttachment{};
    gNormalSpecAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    gNormalSpecAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gNormalSpecAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gNormalSpecAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gNormalSpecAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthAttachmentReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    std::vector<VkAttachmentReference> gAttachmentReferences = {
            {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };

    std::vector<VkAttachmentReference> inputAttachmentReferences = {
            {2, VK_IMAGE_LAYOUT_GENERAL},
            {3, VK_IMAGE_LAYOUT_GENERAL},
            {4, VK_IMAGE_LAYOUT_GENERAL}
    };

    VkSubpassDescription gSubpass{};
    gSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    gSubpass.colorAttachmentCount = gAttachmentReferences.size();
    gSubpass.pColorAttachments = gAttachmentReferences.data();
    gSubpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;
    subpass.inputAttachmentCount = inputAttachmentReferences.size();
    subpass.pInputAttachments = inputAttachmentReferences.data();

    VkSubpassDependency gSubpassDependency{};
    gSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    gSubpassDependency.dstSubpass = 0;
    gSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    gSubpassDependency.srcAccessMask = 0;
    gSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    gSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = 0;
    subpassDependency.dstSubpass = 1;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    subpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDependency depthDependency{};
    depthDependency.srcSubpass = 0;
    depthDependency.dstSubpass = 1;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    depthDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    renderPass.create({colorAttachment, depthAttachment, gColorAttachment, gPositionAttachment, gNormalSpecAttachment}, {gSubpass, subpass}, {gSubpassDependency, subpassDependency, depthDependency});

    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(swapchain.getImageCount());
    for (int i = 0; i < swapchain.getImageCount(); ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i], depthImage->getImageView(), gColor->getImageView(), gPosition->getImageView(), gNormalSpec->getImageView()}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

    auto vertCode = readFile("shaders/vertex.spv");
    auto fragCode = readFile("shaders/fragment.spv");
    auto transparentCode = readFile("shaders/transparent.spv");
    auto deferredCode = readFile("shaders/deferred.spv");
    auto fullscreenQuadCode = readFile("shaders/fullscreenQuad.spv");
    ShaderModule vertModule(device, vertCode), fragModule(device, fragCode), transparentModule(device, transparentCode), deferredModule(device, deferredCode), fullscreenQuadModule(device, fullscreenQuadCode);
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode), transparentShader(transparentCode), deferredShader(deferredCode);
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(vertexShader+fragmentShader);
    VkPipelineLayout deferredLayout = pipelineLayoutCache.createPipelineLayout(deferredShader.getDescriptorSetLayouts(), deferredShader.getPushConstantRanges());

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain.getExtent().width);
    viewport.height = static_cast<float>(swapchain.getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = swapchain.getExtent();

    VkPipeline pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setVertexInputState(Vertex::bindings(), Vertex::attributes())
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setColorBlendState({noBlend, noBlend, noBlend})
            .setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS)
            .setLayout(pipelineLayout)
            .setRenderPass(renderPass, 0)
            .build(device);

    VkPipeline deferredPipeline = GraphicsPipelineBuilder()
            .setShaders(fullscreenQuadModule, deferredModule)
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setLayout(deferredLayout)
            .setRenderPass(renderPass, 1)
            .build(device);

    VkPipeline transparentPipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, transparentModule)
            .setVertexInputState(Vertex::bindings(), Vertex::attributes())
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .setColorBlendState({alphaBlend, noBlend, noBlend})
            .setDepthStencilState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
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
    CommandBuffer cont = commandPool.allocateCommandBuffer();

    Mesh voxelia = Mesh::loadObj("/home/honeywrap/Documents/kitten/assets/dragon.obj");
    voxelia.pushMesh(resourceManager, stagingBufferManager);
    std::cout << "Triangles:" << voxelia.indices.size()/3 << std::endl;

    Texture voxeliaTexture = Texture::loadImage("/home/honeywrap/Documents/kitten/assets/vokselia_spawn/vokselia_spawn.png");
    voxeliaTexture.pushTexture(resourceManager, stagingBufferManager);

    auto ubo = resourceManager.createBuffer({.size = sizeof(glm::mat4x4)*128, .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});

    VkDescriptorSet voxeliaSet;
    VkDescriptorImageInfo descriptorImageInfo = {voxeliaTexture.sampler->getSampler(), voxeliaTexture.image->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo descriptorBufferInfo = {ubo->getBuffer(), 0, sizeof(glm::mat4x4)*3};
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
    .bind_image(0, &descriptorImageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .bind_buffer(1, &descriptorBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .build(voxeliaSet);

    MeshRenderer voxeliaRenderer(voxelia, {
            {pipeline,            pipelineLayout, voxeliaSet, 0},
            {transparentPipeline, pipelineLayout, voxeliaSet, 1}
    });

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator(time(NULL));
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        float scale = (float) i / 64.0;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 64; i++) {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f);
        ssaoNoise.push_back(noise);
    }

    auto noiseImage = resourceManager.createImage({
                                                         .imageType = VK_IMAGE_TYPE_2D,
                                                         .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                         .extent = {8, 8, 1},
                                                         .mipLevels = 1,
                                                         .arrayLayers = 1,
                                                         .samples = VK_SAMPLE_COUNT_1_BIT,
                                                         .tiling = VK_IMAGE_TILING_LINEAR,
                                                         .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                 });

    noiseImage->createImageView({
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = VK_FORMAT_R32G32B32_SFLOAT,
                                         .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
                                 });

    auto noiseSampler = resourceManager.createSampler({.magFilter=VK_FILTER_LINEAR, .minFilter=VK_FILTER_LINEAR});
    stagingBufferManager.stageImageData(ssaoNoise.data(), noiseImage->getImage(), ssaoNoise.size()*sizeof(ssaoNoise[0]), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {}, {4, 4, 1});

    auto ssaoSamples = resourceManager.createBuffer({.size = ssaoKernel.size()*sizeof(ssaoKernel[0]), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});
    stagingBufferManager.stageBufferData(ssaoKernel.data(), ssaoSamples->getBuffer(), ssaoKernel.size()*sizeof(ssaoKernel[0]));

    stagingBufferManager.flush();

    cont.begin();
    ResourceBarrier::transitionImageLayout(cont, voxeliaTexture.image->getImage(), voxeliaTexture.image->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ResourceBarrier::transitionImageLayout(cont, noiseImage->getImage(), noiseImage->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    cont.end();
    cont.submit(device.getGraphicsQueue());

    VkDescriptorSet gBufferSet;
    VkDescriptorImageInfo gDescriptorInfos[] = {
            {VK_NULL_HANDLE, gColor->getImageView(), VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, gPosition->getImageView(), VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, gNormalSpec->getImageView(), VK_IMAGE_LAYOUT_GENERAL},
            {noiseSampler->getSampler(), noiseImage->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    };
    VkDescriptorBufferInfo ssaoKernelDescriptorInfo = {ssaoSamples->getBuffer(), 0, VK_WHOLE_SIZE};
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
    .bind_image(0, &gDescriptorInfos[0], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
    .bind_image(1, &gDescriptorInfos[1], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
    .bind_image(2, &gDescriptorInfos[2], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
    .bind_image(3, &gDescriptorInfos[3], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .bind_buffer(4, &ssaoKernelDescriptorInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build(gBufferSet);

    //Main rendering loop
    uint32_t frame = 0;
    const auto start = std::chrono::high_resolution_clock::now(); //Timestamp for rendering start

    float deltaTime = 0.0f;	// Time between current frame and last frame
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

        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        renderPass.begin(commandBuffer, framebuffers[frame], scissor, {{.color = {0.0f, 0.0f, 0.0f, 0.0f}}, {.depthStencil = {1.0f, 0}}, {.color = {0.75f, 0.76f, 0.77f, 0.0f}}, {}, {}});

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

        glm::mat4 model = glm::scale(glm::vec3(1.0f,1.0f,1.0f))*glm::rotate(glm::radians(time*2.5f*0), glm::vec3(0.0f, 1.0f, 0.0f))*glm::translate(glm::vec3(0,0,0));
        glm::mat4 view = glm::lookAt(camera, camera+glm::normalize(direction), glm::vec3(0,1,0));
        glm::mat4 projection = glm::perspective(glm::radians(75.0f), (float)width/(float)height, 0.001f, 5000.0f);
        projection[1][1] *= -1;

        glm::mat4 ubo_data[] = {model, view, projection};
        stagingBufferManager.stageBufferData(&ubo_data, ubo->getBuffer(), sizeof(glm::mat4)*3);
        stagingBufferManager.flush();

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(time), &time);

        VkPipeline boundPipeline = VK_NULL_HANDLE;

        voxeliaRenderer.render(commandBuffer, boundPipeline, 0);

        voxeliaRenderer.render(commandBuffer, boundPipeline, 1);

        vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        glm::ivec2 screenSize(width, height);

        vkCmdPushConstants(commandBuffer, deferredLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(projection), &projection);
        vkCmdPushConstants(commandBuffer, deferredLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(projection), sizeof(screenSize), &screenSize);
        vkCmdPushConstants(commandBuffer, deferredLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(projection)+sizeof(screenSize), sizeof(time), &time);

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipeline);
        commandBuffer.bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, deferredLayout, 0, {gBufferSet});
        commandBuffer.draw(6);

        renderPass.end(commandBuffer);
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

    vkDestroyPipeline(device, deferredPipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    swapchain.destroy();
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwTerminate();
    return 0;
}