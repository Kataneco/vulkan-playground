#pragma once

#include <filesystem>

#include "app/EditorUI.h"
#include "app/Renderer.h"
#include "app/ViewportRenderer.h"
#include "descriptor/DescriptorSetManager.h"
#include "pipeline/PipelineLayoutManager.h"
#include "rendering/CommandPool.h"
#include "resource/MemoryAllocator.h"
#include "resource/ResourceManager.h"
#include "resource/StagingBufferManager.h"

class Application {
public:
    Application(int argc, char** argv);
    ~Application();

    void run();

private:
    void initializeAssets(int argc, char** argv);

    std::filesystem::path assetRoot;
    Renderer renderer;
    DescriptorLayoutCache descriptorLayoutCache;
    DescriptorAllocator descriptorAllocator;
    MemoryAllocator memoryAllocator;
    ResourceManager resourceManager;
    StagingBufferManager stagingBufferManager;
    PipelineLayoutCache pipelineLayoutCache;
    EditorUI editorUI;
    ViewportRenderer viewportRenderer;
};
