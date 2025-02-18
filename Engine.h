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
#include "experimental/voxel.h"

constexpr VkPipelineColorBlendAttachmentState noBlend{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
};

constexpr VkPipelineColorBlendAttachmentState alphaBlend = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
};

class Engine {
public:
    explicit Engine();
    ~Engine();
private:
};