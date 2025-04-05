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
    int x0y0z0;
    int x1y0z0;
    int x0y1z0;
    int x1y1z0;
    int x0y0z1;
    int x1y0z1;
    int x0y1z1;
    int x1y1z1;
};