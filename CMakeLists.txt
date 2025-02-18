cmake_minimum_required(VERSION 3.29)
project(kitten)

set(CMAKE_CXX_STANDARD 26)

find_package(glm)
find_package(glfw3)

include_directories(SYSTEM lib)
include_directories(src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -march=native")

add_executable(kitten main.cpp
        src/core/VulkanInstance.cpp
        src/core/VulkanInstance.h
        src/core/Device.cpp
        src/core/Device.h
        src/core/Swapchain.cpp
        src/core/Swapchain.h
        src/pipeline/GraphicsPipelineBuilder.cpp
        src/pipeline/GraphicsPipelineBuilder.h
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
        src/resource/Resource.cpp
        src/resource/Resource.h
        src/resource/ResourceManager.cpp
        src/resource/ResourceManager.h
        src/resource/MemoryAllocator.cpp
        src/resource/MemoryAllocator.h
        src/resource/StagingBufferManager.cpp
        src/resource/StagingBufferManager.h
        src/resource/ResourceBarrier.cpp
        src/resource/ResourceBarrier.h
        lib/spirv_reflect.c
        Engine.cpp
        Engine.h
        src/core/Queue.cpp
        src/core/Queue.h
        src/scene/MeshRenderer.cpp
        src/scene/MeshRenderer.h
        lib/stb_image.h
        src/util/Window.cpp
        src/util/Window.h
        src/resource/Texture.cpp
        src/resource/Texture.h
        src/experimental/voxel.cpp
        src/experimental/voxel.h
)

target_link_libraries(kitten glfw)

#Shader compilation
find_program(GLSLC_EXECUTABLE glslc)

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(COMPILED_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB SHADER_FILES "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp")

foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    set(COMPILED_SHADER_FILE "${COMPILED_SHADER_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
            OUTPUT ${COMPILED_SHADER_FILE}
            COMMAND ${GLSLC_EXECUTABLE} ${SHADER_FILE} -o ${COMPILED_SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling shader ${SHADER_NAME}"
            VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${COMPILED_SHADER_FILE})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(kitten compile_shaders)
