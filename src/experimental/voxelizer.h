#pragma once
#include "util/VulkanUtils.h"

struct OctreeNode {
    int parent;
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