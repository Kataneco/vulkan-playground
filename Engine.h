#pragma once
#include "util/VulkanUtils.h"
#include "core/VulkanInstance.h"
#include "core/Device.h"
#include "core/Swapchain.h"
#include "descriptor/DescriptorSetManager.h"
#include "pipeline/PipelineLayoutManager.h"
#include "pipeline/GraphicsPipelineBuilder.h"
#include "rendering/CommandPool.h"
#include "rendering/CommandBuffer.h"
#include "rendering/RenderPass.h"
#include "rendering/Framebuffer.h"
#include "resource/MemoryAllocator.h"
#include "resource/ResourceManager.h"
#include "resource/Buffer.h"
#include "resource/Image.h"
#include "resource/Sampler.h"
#include "shader/ShaderModule.h"
#include "shader/ShaderReflection.h"
#include "sync/Fence.h"
#include "sync/Semaphore.h"
#include "scene/Mesh.h"
#include "scene/Material.h"
#include "scene/Scene.h"
#include "core/Queue.h"
#include "resource/StagingBufferManager.h"
#include "scene/MeshRenderer.h"
#include "util/Window.h"
#include "resource/Texture.h"
#include "resource/ResourceBarrier.h"

class Engine {
public:
    explicit Engine();
    ~Engine();
private:
};