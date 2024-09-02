cmake_minimum_required(VERSION 3.29)
project(kitten)

set(CMAKE_CXX_STANDARD 26)

find_package(glm)
find_package(glfw3)

include_directories(lib)
include_directories(src)

add_executable(kitten main.cpp
        src/core/VulkanInstance.cpp
        src/core/VulkanInstance.h
        src/core/Device.cpp
        src/core/Device.h
        src/core/SwapchainManager.cpp
        src/core/SwapchainManager.h
        src/pipeline/GraphicsPipelineBuilder.cpp
        src/pipeline/GraphicsPipelineBuilder.h
        src/pipeline/PipelineCache.cpp
        src/pipeline/PipelineCache.h
        src/pipeline/PipelineLayoutManager.cpp
        src/pipeline/PipelineLayoutManager.h
        src/descriptor/DescriptorSetManager.cpp
        src/descriptor/DescriptorSetManager.h
        src/shader/ShaderModule.cpp
        src/shader/ShaderModule.h
        src/shader/ShaderReflection.cpp
        src/shader/ShaderReflection.h
        src/rendering/RenderPass.cpp
        src/rendering/RenderPass.h
        src/rendering/Framebuffer.cpp
        src/rendering/Framebuffer.h
        src/rendering/CommandPool.cpp
        src/rendering/CommandPool.h
        src/rendering/CommandBuffer.cpp
        src/rendering/CommandBuffer.h
        src/scene/Material.cpp
        src/scene/Material.h
        src/scene/Mesh.cpp
        src/scene/Mesh.h
        src/scene/Scene.cpp
        src/scene/Scene.h
        src/resource/Buffer.cpp
        src/resource/Buffer.h
        src/resource/Image.cpp
        src/resource/Image.h
        src/resource/Sampler.cpp
        src/resource/Sampler.h
        src/sync/Semaphore.cpp
        src/sync/Semaphore.h
        src/sync/Fence.cpp
        src/sync/Fence.h
        src/util/VulkanUtils.cpp
        src/util/VulkanUtils.h
        src/util/ErrorHandling.cpp
        src/util/ErrorHandling.h
        src/resource/Resource.cpp
        src/resource/Resource.h
        src/resource/ResourceManager.cpp
        src/resource/ResourceManager.h
        src/resource/MemoryAllocator.cpp
        src/resource/MemoryAllocator.h
        src/resource/StagingBufferManager.cpp
        src/resource/StagingBufferManager.h
        src/resource/ResourceTracker.cpp
        src/resource/ResourceTracker.h
        src/resource/ResourceBarrier.cpp
        src/resource/ResourceBarrier.h
        lib/spirv_reflect.c)

target_link_libraries(kitten glfw)