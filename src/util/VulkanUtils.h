#pragma once

#ifdef KITTY_MAIN
#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJ_LOADER_OPT_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <memory>
#include <algorithm>
#include <queue>

#define VK_NO_PROTOTYPES
#include "volk.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

//#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include "spirv_reflect.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"
#include "stb_image.h"

size_t hash_combine(size_t lhs, size_t rhs);

std::string readFile(const char* filePath);

struct DescriptorSetLayoutData {
    uint32_t set;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bool operator==(const DescriptorSetLayoutData &other) const;
    VkDescriptorSetLayoutCreateInfo getCreateInfo();
};

namespace std {
    template<> struct hash<DescriptorSetLayoutData> {
        size_t operator()(DescriptorSetLayoutData const &data) const {
            size_t result = hash<size_t>()(data.bindings.size());
            for (const auto &b: data.bindings) {
                size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
                result = hash_combine(result, binding_hash);
            }
            return result;
        }
    };
}

