#pragma once

#include <filesystem>
#include <string>

#include <imnodes.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "app/NodeEditor.h"
#include "app/Renderer.h"
#include "app/ShaderWorkspace.h"
#include "shader/ShaderCompiler.h"

struct ViewportPanelInfo {
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkExtent2D textureExtent{0, 0};
};

class EditorUI {
public:
    struct Config {
        std::filesystem::path fontPath;
    };

    EditorUI(VulkanInstance& instance, Device& device, Renderer& renderer, const Config& config);
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void newFrame();
    ImVec2 draw(const ViewportPanelInfo& viewportInfo);
    ImDrawData* render();

    VkDescriptorSet registerTexture(VkSampler sampler, VkImageView imageView, VkImageLayout layout);

    VulkanNodeEditor& getNodeEditor() { return nodeEditor; }
    ShaderWorkspace& getWorkspace() { return shaderWorkspace; }
    ShaderCompiler& getCompiler() { return shaderCompiler; }

private:
    void configureStyle();
    void setupDockspace();
    void drawTextWorkspacePanel();
    void updateStatusBar(float deltaTime);

    VulkanInstance& instance;
    Device& device;
    Renderer& renderer;
    Config config;

    ShaderWorkspace shaderWorkspace;
    ShaderCompiler shaderCompiler;
    VulkanNodeEditor nodeEditor;
    std::string statusMessage;
    float statusTimer = 0.0f;
    bool dockspaceConfigured = false;
};
