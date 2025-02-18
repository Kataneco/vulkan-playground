#pragma once
#include "util/VulkanUtils.h"

struct Voxel {
    uint32_t morton; //interleaved xyz
    uint32_t section; //extension
    uint32_t color; //4x8bit RGBA
    //uint64_t normal; //3x16bit unorm + 1x16bit flags
    glm::vec3 normal;
};

struct OctreeNode {
    uint32_t children[8];
    uint32_t color;
    uint32_t flags;
};