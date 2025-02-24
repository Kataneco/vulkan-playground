#pragma once
#include "util/VulkanUtils.h"

struct VoxelizerData {
    glm::vec3 center;
    glm::vec3 resolution;
};

struct Voxel {
    glm::vec3 position;
    uint32_t normal;
    uint32_t color;
};