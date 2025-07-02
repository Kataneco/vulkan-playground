#pragma once
#include "util/VulkanUtils.h"

#include "voxel.h"
#include "../Engine.h"

class VoxelGrid {
public:
    explicit VoxelGrid(VkDevice device, VoxelizerData options);
    ~VoxelGrid();

    VoxelizerData voxelizerData;

    std::shared_ptr<Buffer> voxelBuffer;
    std::shared_ptr<Buffer> svoBuffer;
    std::shared_ptr<Buffer> voxelCountBuffer;
    std::shared_ptr<Buffer> nodeCountBuffer;

    RenderPass renderPass;
    std::shared_ptr<Image> dummyImage;
    Framebuffer framebuffer;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
private:
};