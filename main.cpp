#define KITTY_MAIN
#include <future>

#include "Engine.h"
#include "imgui_internal.h"

#include <random>

#include "src/experimental/voxelizer.h"
#include "src/alphyslab/node.h"
#include <future>

#include "ui.h"
#include "TextEditor.h"
#include "imstyles.h"

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

int main(int argc, char* argv[]) {
    std::cout << "Hello, darlings!" << std::endl;
    srand(time(nullptr));

    glfwSetErrorCallback(glfw_error_callback);
    Window::initialize();

    std::vector<const char*> instanceExtensions = Window::getRequiredInstanceExtensions();
    VulkanInstance instance(VK_API_VERSION_1_3, instanceExtensions, true);
    Device device(instance, {.independentBlend = VK_TRUE, .geometryShader = VK_TRUE, .fillModeNonSolid = VK_TRUE, .wideLines = VK_TRUE, .samplerAnisotropy = VK_TRUE, .vertexPipelineStoresAndAtomics = VK_TRUE, .fragmentStoresAndAtomics = VK_TRUE}, {});

    Texture icon = Texture::loadImage("/home/honeywrap/Documents/kitten/assets/icon.png");
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    Window window(1600, 900);
    window.setWindowIcon(icon, icon);

    DescriptorLayoutCache descriptorLayoutCache(device);
    DescriptorAllocator descriptorAllocator(device);
    MemoryAllocator memoryAllocator(instance, device);
    ResourceManager resourceManager(device, memoryAllocator);
    StagingBufferManager stagingBufferManager(device, 64 * 1024 * 1024);
    CommandPool commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    PipelineLayoutCache pipelineLayoutCache(device, descriptorLayoutCache);

    Swapchain swapchain(device, window);

    RenderPass renderPass(device);
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

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    renderPass.create({colorAttachment}, {subpass}, {subpassDependency});

    std::vector<Framebuffer> framebuffers;
    framebuffers.reserve(swapchain.getImageCount());
    for (size_t i = 0; i < swapchain.getImageCount(); ++i) {
        framebuffers.emplace_back(device, renderPass);
        framebuffers.back().create({swapchain.getImageViews()[i]}, swapchain.getExtent().width, swapchain.getExtent().height);
    }

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
    auto objectData = resourceManager.createBuffer({.size = sizeof(glm::mat4x4)*1024, .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});

    auto cameraData = resourceManager.createBuffer({.size = sizeof(glm::mat4x4)*2, .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT});

    VkDescriptorSet cameraSet;
    VkDescriptorBufferInfo cameraSetInfo = {cameraData->getBuffer(), 0, sizeof(glm::mat4x4)*2};
    DescriptorBuilder(descriptorLayoutCache, descriptorAllocator)
            .bind_buffer(0, &cameraSetInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(cameraSet);

    //NOTE: Mega experimental
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

    // TODO: move styling to headers
    globalStyleConfig();

    //io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromFileTTF("/usr/share/fonts/TTF/HackNerdFontMono-Regular.ttf");

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);

    //void* instance_device[2] = {VkInstance(instance), VkDevice(device)};
    //ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char *function_name, void *data) {void** idarr = *reinterpret_cast<void ***>(data); PFN_vkVoidFunction instanceAddr = vkGetInstanceProcAddr(static_cast<VkInstance>(idarr[0]), function_name); PFN_vkVoidFunction deviceAddr = vkGetDeviceProcAddr(static_cast<VkDevice>(idarr[1]), function_name); return deviceAddr ? deviceAddr : instanceAddr; }, &instance_device);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = instance;
    init_info.PhysicalDevice = device;
    init_info.Device = device;
    init_info.QueueFamily = device.getGraphicsFamily();
    init_info.Queue = device.getGraphicsQueue();
    init_info.DescriptorPoolSize = 512;
    init_info.MinImageCount = swapchain.getImageCount();
    init_info.ImageCount = swapchain.getImageCount();
    init_info.Allocator = nullptr;
    init_info.PipelineInfoMain.RenderPass = renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    // Text editor
    TextEditor editor;
    auto lang = TextEditor::LanguageDefinition::GLSL();
    editor.SetLanguageDefinition(lang);

    // Node editor
    VulkanNodeEditor nodeEditor(instance, device);
    nodeEditor.SetTextEditor(&editor);  // NEW: Connect text editor to node editor

    // Viewport
    auto meowImage = resourceManager.createImage({
          .imageType = VK_IMAGE_TYPE_2D,
          .format = VK_FORMAT_B8G8R8A8_SRGB,
          .extent = {2048, 2048, 1},
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_OPTIMAL,
          .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    });

    meowImage->createImageView({.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = meowImage->getFormat(), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}});

    RenderPass meowRenderPass(device);
    VkAttachmentDescription meowAttachment{};
    meowAttachment.format = meowImage->getFormat();
    meowAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    meowAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    meowAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    meowAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference meowAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription meowSubpass{};
    meowSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    meowSubpass.colorAttachmentCount = 1;
    meowSubpass.pColorAttachments = &meowAttachmentReference;

    VkSubpassDependency meowSubpassDependency{};
    meowSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    meowSubpassDependency.dstSubpass = 0;
    meowSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    meowSubpassDependency.srcAccessMask = 0;
    meowSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    meowSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    meowRenderPass.create({meowAttachment}, {meowSubpass}, {meowSubpassDependency});

    auto vertCode = readFile("shaders/fullscreenQuad.vert.spv");
    auto fragCode = readFile("shaders/meow.frag.spv");
    ShaderModule vertModule(device, vertCode), fragModule(device, fragCode);
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(vertexShader+fragmentShader);

    VkViewport viewport{};
    viewport.width = 2048;
    viewport.height = 2048;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = {2048, 2048};

    VkPipeline pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setViewportState(viewport, scissor)
            .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 1.0f)
            .setColorBlendState({alphaBlend})
            .setLayout(pipelineLayout)
            .setRenderPass(meowRenderPass, 0)
            .setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
            .build(device);

    Framebuffer meowFramebuffer(device, meowRenderPass);
    meowFramebuffer.create({meowImage->getImageView()}, static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height));

    auto meowSampler = resourceManager.createSampler({});

    VkDescriptorSet meowTargetSet = ImGui_ImplVulkan_AddTexture(meowSampler->getSampler(), meowImage->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //Main rendering loop
    uint32_t frame = 0;
    const auto start = std::chrono::high_resolution_clock::now(); //Timestamp for rendering start

    float deltaTime = 0.0f;    // Time between current frame and last frame
    float currentFrame = 0.0f; // Time of current frame
    float lastFrame = 0.0f; // Time of last frame

    //Camera
    float sensitivity = 0.1f;
    float lastX = 0.0f, lastY = 0.0f;
    glm::vec3 camera = {0.0f, 0.0f, -1.0f};
    ImVec2 nyan; bool purr = false;
    while (!window.windowShouldClose()) {
        window.pollEvents(); //TODO bad design
        if (window.windowIconified()) continue; //Pause rendering if minimized

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

        glm::mat4 view = glm::lookAt(camera, camera+glm::vec3(0,0,1), glm::vec3(0,1,0));
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(swapchain.getExtent().width), 0.0f, static_cast<float>(swapchain.getExtent().height), -100.0f, 100.0f);

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::GetID("Dockspace");
        ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

        static auto first_time = true;
        if (first_time) {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetWindowSize());

            ImGuiID dockspace_main_id = dockspace_id;
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dockspace_main_id, ImGuiDir_Right, 0.42f, nullptr, &dockspace_main_id);
            ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.42f, nullptr, &dock_right);
            ImGuiID dock_text = ImGui::DockBuilderSplitNode(dockspace_main_id, ImGuiDir_Left, 0.95f, nullptr, &dockspace_main_id);

            ImGui::DockBuilderDockWindow("Viewport", dock_down);
            ImGui::DockBuilderDockWindow("Shader Graph Editor", dockspace_main_id);
            ImGui::DockBuilderDockWindow("Pipeline Graph Editor", dockspace_main_id);
            ImGui::DockBuilderDockWindow("Node Properties", dock_right);
            ImGui::DockBuilderDockWindow("Hierarchy", dock_right);
            ImGui::DockBuilderDockWindow("Text Editor", dock_text);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        commandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        nodeEditor.Draw(commandBuffer);

        ImGui::Begin("Viewport");
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if (nodeEditor.getFocusedImage() == VK_NULL_HANDLE) {
            ImGui::Image(meowTargetSet, viewportSize, ImVec2(0,0), ImVec2(viewportSize.x/2048, viewportSize.y/2048));
        } else {
            ImGui::Image(nodeEditor.getFocusedImage(), viewportSize, ImVec2(0,0), ImVec2(viewportSize.x/2048, viewportSize.y/2048));
        }
        //ImGui::Text("size = %d x %d", static_cast<int>(viewportSize.x), static_cast<int>(viewportSize.y));
        ImGui::End();

        ImGui::Begin("Text Editor");
        auto cpos = editor.GetCursorPosition();

        if (ImGui::Button("Save")) {
            nodeEditor.SaveCurrentShaderEdit();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Auto-saves on node switch)");

        editor.Render("Shader Editor");
        ImGui::End();

        ImGui::Render();
        ImDrawData* main_draw_data = ImGui::GetDrawData();

        meowRenderPass.begin(commandBuffer, meowFramebuffer, {.extent = {static_cast<uint32_t>(viewportSize.x)*0+2048, 2048+0*static_cast<uint32_t>(viewportSize.y)}}, {{.color = {0.0f, 0.0f, 0.0f, 0.0f}}, {.depthStencil = {1.0f, 0}}});

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        glm::vec4 data = {viewportSize.x, viewportSize.y, time, deltaTime};
        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        commandBuffer.pushConstants(pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
        commandBuffer.draw(3);

        meowRenderPass.end(commandBuffer);

        renderPass.begin(commandBuffer, framebuffers[swapchainIndex], {.extent = swapchain.getExtent()}, {{.color = {0.0f, 0.0f, 0.0f, 0.0f}}, {.depthStencil = {1.0f, 0}}});

        ImGui_ImplVulkan_RenderDrawData(main_draw_data, commandBuffer);

        renderPass.end(commandBuffer);

        commandBuffer.end();

        commandBuffer.submit(device.getGraphicsQueue(), {swapchainLockSemaphore[frame]}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}, {renderLockSemaphore[frame]}, frameLockFence[frame]);

        if (swapchain.present(swapchainIndex, renderLockSemaphore[frame]) == 1) {
            framebuffers.clear();
            framebuffers.reserve(swapchain.getImageCount());

            for (int i = 0; i < swapchain.getImageCount(); ++i) {
                framebuffers.emplace_back(device, renderPass);
                framebuffers.back().create({swapchain.getImageViews()[i]}, swapchain.getExtent().width, swapchain.getExtent().height);
            }

            viewport.width = static_cast<float>(swapchain.getExtent().width);
            viewport.height = static_cast<float>(swapchain.getExtent().height);
            scissor.extent = swapchain.getExtent();
        }

        frame = (frame+1) % swapchain.getImageCount(); //Jump to next frame
    }

    device.waitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();

    vkDestroyPipeline(device, pipeline, nullptr);
    //Window::terminate();
    return 0;
}