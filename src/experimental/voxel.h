#pragma once
#include "util/VulkanUtils.h"

struct VoxelizerData {
    alignas(16)glm::vec3 center;
    alignas(16)glm::vec3 resolution;
};

struct Voxel {
    uint32_t position;
    uint32_t normal;
    uint32_t color;
};

struct OctreeNode {
    int parent;
    int childCount;
    int nodes[8];
};