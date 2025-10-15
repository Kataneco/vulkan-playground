#include "Application.h"

#include <chrono>
#include <filesystem>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui_impl_vulkan.h>

namespace {
std::filesystem::path determineAssetRoot(int argc, char** argv) {
    if (argc > 0 && argv && argv[0]) {
        std::filesystem::path executable = std::filesystem::absolute(argv[0]).parent_path();
        std::filesystem::path candidate = executable / "assets";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    std::filesystem::path fallback = std::filesystem::current_path() / "assets";
    if (std::filesystem::exists(fallback)) {
        return fallback;
    }

    return {};
}

RendererCreateInfo buildRendererInfo(const std::filesystem::path& assetRoot) {
    RendererCreateInfo info;
    info.width = 1600;
    info.height = 900;
    info.title = "Kitten Editor";
    info.enableValidation = true;

    VkPhysicalDeviceFeatures features{};
    features.independentBlend = VK_TRUE;
    features.geometryShader = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.wideLines = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    features.vertexPipelineStoresAndAtomics = VK_TRUE;
    features.fragmentStoresAndAtomics = VK_TRUE;
    info.enabledFeatures = features;

    if (!assetRoot.empty()) {
        std::filesystem::path icon = assetRoot / "icon.png";
        if (std::filesystem::exists(icon)) {
            info.iconPath = icon.string();
        }
    }

    return info;
}

EditorUI::Config buildEditorConfig(const std::filesystem::path& assetRoot) {
    EditorUI::Config config{};
    if (!assetRoot.empty()) {
        std::filesystem::path fontPath = assetRoot / "fonts" / "HackNerdFontMono-Regular.ttf";
        if (std::filesystem::exists(fontPath)) {
            config.fontPath = fontPath;
        }
    }
    return config;
}
}

Application::Application(int argc, char** argv)
    : assetRoot(determineAssetRoot(argc, argv))
    , renderer(buildRendererInfo(assetRoot))
    , descriptorLayoutCache(renderer.getDevice())
    , descriptorAllocator(renderer.getDevice())
    , memoryAllocator(renderer.getInstance(), renderer.getDevice())
    , resourceManager(renderer.getDevice(), memoryAllocator)
    , stagingBufferManager(renderer.getDevice(), 64 * 1024 * 1024)
    , pipelineLayoutCache(renderer.getDevice(), descriptorLayoutCache)
    , editorUI(renderer.getInstance(), renderer.getDevice(), renderer, buildEditorConfig(assetRoot))
    , viewportRenderer(renderer.getDevice(), resourceManager, pipelineLayoutCache, editorUI, 2048, 2048) {
}

Application::~Application() = default;

void Application::run() {
    auto& window = renderer.getWindow();
    const auto startTime = std::chrono::high_resolution_clock::now();
    float lastFrameTime = 0.0f;

    while (!window.windowShouldClose()) {
        window.pollEvents();
        if (window.windowIconified()) {
            continue;
        }

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        float elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - startTime).count();

        auto frame = renderer.beginFrame();
        if (!frame.has_value()) {
            continue;
        }

        editorUI.newFrame();
        ImVec2 viewportSize = editorUI.draw({viewportRenderer.getViewportDescriptor(), viewportRenderer.getTextureExtent()});
        ImDrawData* drawData = editorUI.render();

        viewportRenderer.record(frame->commandBuffer, frame->extent, elapsedSeconds, deltaTime, viewportSize);

        const std::vector<VkClearValue> clearValues = {VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}}};
        renderer.beginSwapchainRenderPass(*frame, clearValues);
        ImGui_ImplVulkan_RenderDrawData(drawData, frame->commandBuffer);
        renderer.endSwapchainRenderPass(*frame);

        renderer.submitFrame(*frame);
    }

    renderer.getDevice().waitIdle();
}
