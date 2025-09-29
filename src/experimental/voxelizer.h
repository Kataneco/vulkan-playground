#pragma once
#include "util/VulkanUtils.h"

struct OctreeNode {
    int parent;
    uint32_t occlusion; // amount of total children
    uint32_t emission; // pointer to voxel that sums up all child voxels
    uint32_t generation; // node age
    int children[8];
};

struct Voxel {
    uint32_t normal;
    uint32_t color;
};

struct VoxelizerData {
    alignas(16)glm::vec3 center;
    alignas(16)glm::vec3 resolution;
};