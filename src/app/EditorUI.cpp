#include "EditorUI.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <misc/cpp/imgui_stdlib.h>

#include <GLFW/glfw3.h>

EditorUI::EditorUI(VulkanInstance& instance, Device& device, Renderer& renderer, const Config& config)
    : instance(instance)
    , device(device)
    , renderer(renderer)
    , config(config)
    , nodeEditor(instance, device, shaderWorkspace) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();

    configureStyle();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (!config.fontPath.empty() && std::filesystem::exists(config.fontPath)) {
        io.Fonts->AddFontFromFileTTF(config.fontPath.string().c_str(), 14.0f);
    } else {
        io.Fonts->AddFontDefault();
    }

    ImGui_ImplGlfw_InitForVulkan(renderer.getWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = device;
    initInfo.Device = device;
    initInfo.QueueFamily = device.getGraphicsFamily();
    initInfo.Queue = device.getGraphicsQueue();
    initInfo.DescriptorPoolSize = 128;
    initInfo.MinImageCount = renderer.getSwapchain().getImageCount();
    initInfo.ImageCount = renderer.getSwapchain().getImageCount();
    initInfo.Allocator = nullptr;
    initInfo.PipelineInfoMain.RenderPass = renderer.getRenderPass();
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = nullptr;

    ImGui_ImplVulkan_Init(&initInfo);
}

EditorUI::~EditorUI() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}

void EditorUI::configureStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = 1.0f;
    style.WindowRounding = 3;
    style.GrabRounding = 1;
    style.GrabMinSize = 20;
    style.FrameRounding = 3;
    style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
}

void EditorUI::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    setupDockspace();
}

void EditorUI::setupDockspace() {
    ImGuiID dockspaceId = ImGui::GetID("Dockspace");
    ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport());

    if (dockspaceConfigured) {
        return;
    }

    dockspaceConfigured = true;
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

    ImGuiID dockspaceMainId = dockspaceId;
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockspaceMainId, ImGuiDir_Right, 0.42f, nullptr, &dockspaceMainId);
    ImGuiID dockDown = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.42f, nullptr, &dockRight);
    ImGuiID dockText = ImGui::DockBuilderSplitNode(dockspaceMainId, ImGuiDir_Left, 0.95f, nullptr, &dockspaceMainId);

    ImGui::DockBuilderDockWindow("Viewport", dockDown);
    ImGui::DockBuilderDockWindow("Shader Graph Editor", dockspaceMainId);
    ImGui::DockBuilderDockWindow("Node Properties", dockRight);
    ImGui::DockBuilderDockWindow("Text Editor", dockText);
    ImGui::DockBuilderFinish(dockspaceId);
}

ImVec2 EditorUI::draw(const ViewportPanelInfo& viewportInfo) {
    updateStatusBar(ImGui::GetIO().DeltaTime);
    nodeEditor.Draw();

    ImVec2 viewportSize = ImVec2(0, 0);
    ImGui::Begin("Viewport");
    ViewportPanelInfo finalInfo = viewportInfo;
    if (auto preview = nodeEditor.getFocusedPreview()) {
        finalInfo.descriptorSet = preview->descriptorSet;
        finalInfo.textureExtent = preview->extent;
    }
    viewportSize = ImGui::GetContentRegionAvail();
    if (finalInfo.descriptorSet != VK_NULL_HANDLE) {
        ImGui::Image(finalInfo.descriptorSet, viewportSize,
                     ImVec2(0, 0),
                     ImVec2(viewportSize.x / static_cast<float>(finalInfo.textureExtent.width),
                            viewportSize.y / static_cast<float>(finalInfo.textureExtent.height)));
    }
    ImGui::End();

    drawTextWorkspacePanel();

    return viewportSize;
}

ImDrawData* EditorUI::render() {
    ImGui::Render();
    return ImGui::GetDrawData();
}

VkDescriptorSet EditorUI::registerTexture(VkSampler sampler, VkImageView imageView, VkImageLayout layout) {
    return ImGui_ImplVulkan_AddTexture(sampler, imageView, layout);
}

void EditorUI::drawTextWorkspacePanel() {
    if (!ImGui::Begin("Text Editor")) {
        ImGui::End();
        return;
    }

    if (!statusMessage.empty()) {
        ImGui::TextWrapped("%s", statusMessage.c_str());
        ImGui::Separator();
    }

    auto& documents = shaderWorkspace.getDocuments();
    if (documents.empty()) {
        ImGui::TextDisabled("No shader documents open. Create a shader node to begin editing.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("ShaderWorkspaceTabs", ImGuiTabBarFlags_Reorderable)) {
        for (auto& docPtr : documents) {
            auto& document = *docPtr;
            ImGuiTabItemFlags tabFlags = 0;
            if (&document == shaderWorkspace.getActiveDocument()) {
                tabFlags |= ImGuiTabItemFlags_SetSelected;
            }

            if (ImGui::BeginTabItem(document.tabLabel().c_str(), nullptr, tabFlags)) {
                shaderWorkspace.setActiveDocument(document.nodeId);
                ImGui::PushID(document.nodeId);

                float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                ImVec2 contentRegion = ImGui::GetContentRegionAvail();

                if (ImGui::Button("Save")) {
                    std::string error;
                    std::filesystem::path targetPath = document.pendingPathInput.empty() ? document.filePath : std::filesystem::path(document.pendingPathInput);
                    if (targetPath.empty()) {
                        statusMessage = "Provide a file path before saving.";
                        statusTimer = 3.0f;
                    } else {
                        bool ok = shaderWorkspace.saveAs(document, targetPath, &error);
                        if (ok) {
                            statusMessage = "Saved " + targetPath.string();
                            statusTimer = 3.0f;
                        } else {
                            statusMessage = "Save failed: " + error;
                            statusTimer = 5.0f;
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Compile")) {
                    auto result = shaderWorkspace.compile(document, shaderCompiler);
                    if (result.success) {
                        statusMessage = result.message.empty()
                                           ? "Compilation succeeded"
                                           : result.message;
                        statusTimer = 3.0f;
                        nodeEditor.onShaderCompiled(document.nodeId, document.stage, document.spirv, document.entryPoint);
                    } else {
                        statusMessage = result.message.empty() ? "Compilation failed" : result.message;
                        statusTimer = 5.0f;
                    }
                }
                ImGui::SameLine();
                ImGui::Checkbox("Auto-compile", &document.autoCompile);

                if (ImGui::InputText("Path", &document.pendingPathInput)) {
                    document.hasPendingSave = true;
                }
                if (ImGui::InputText("Entry Point", &document.entryPoint)) {
                    // no-op, entry point stored directly
                }

                if (!document.compileMessage.empty()) {
                    ImVec4 color = document.compileSucceeded ? ImVec4(0.4f, 1.0f, 0.6f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextWrapped("%s", document.compileMessage.c_str());
                    ImGui::PopStyleColor();
                }

                float editorHeight = contentRegion.y - (lineHeight * 4.0f);
                editorHeight = std::max(editorHeight, 100.0f);
                std::string editorLabel = "ShaderEditor##" + std::to_string(document.nodeId);
                document.editor.Render(editorLabel.c_str(), ImVec2(contentRegion.x, editorHeight));
                if (document.editor.IsTextChanged()) {
                    document.dirty = true;
                    if (document.autoCompile) {
                        auto result = shaderWorkspace.compile(document, shaderCompiler);
                        if (result.success) {
                            nodeEditor.onShaderCompiled(document.nodeId, document.stage, document.spirv, document.entryPoint);
                        }
                    }
                }

                ImGui::PopID();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorUI::updateStatusBar(float deltaTime) {
    if (statusTimer > 0.0f) {
        statusTimer -= deltaTime;
        if (statusTimer <= 0.0f) {
            statusTimer = 0.0f;
            statusMessage.clear();
        }
    }
}
